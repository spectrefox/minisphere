﻿using System;
using System.Collections.Concurrent;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Net.Sockets;
using System.Threading;
using System.Threading.Tasks;

using Sphere.Plugins;
using Sphere.Plugins.Interfaces;
using Sphere.Plugins.Views;
using minisphere.Gdk.Forms;
using minisphere.Gdk.Debugger;

namespace minisphere.Gdk.Plugins
{
    class SsjDebugger : IDebugger, IDisposable
    {
        private string sgmPath;
        private Process engineProcess;
        private string engineDir;
        private bool haveError = false;
        private ConcurrentQueue<dynamic[]> replies = new ConcurrentQueue<dynamic[]>();
        private Timer focusTimer;
        private string sourcePath;
        private Timer updateTimer;
        private bool expectDetach = false;
        private PluginMain plugin;

        public SsjDebugger(PluginMain main, string gamePath, string enginePath, Process engine, IProject project)
        {
            plugin = main;
            sgmPath = gamePath;
            sourcePath = project.RootPath;
            engineProcess = engine;
            engineDir = Path.GetDirectoryName(enginePath);
            focusTimer = new Timer(HandleFocusSwitch, this, Timeout.Infinite, Timeout.Infinite);
            updateTimer = new Timer(UpdateDebugViews, this, Timeout.Infinite, Timeout.Infinite);
        }

        public void Dispose()
        {
            Inferior.Dispose();
            focusTimer.Dispose();
            updateTimer.Dispose();
        }

        public Inferior Inferior { get; private set; }
        public string FileName { get; private set; }
        public int LineNumber { get; private set; }
        public bool Running { get; private set; }

        public event EventHandler Attached;
        public event EventHandler Detached;
        public event EventHandler<PausedEventArgs> Paused;
        public event EventHandler Resumed;

        public async Task<bool> Attach()
        {
            try
            {
                await Connect("localhost", 1208);
                ++plugin.Sessions;
                return true;
            }
            catch (TimeoutException)
            {
                return false;
            }
        }

        public async Task Detach()
        {
            expectDetach = true;
            await Inferior.Detach();
            Dispose();
        }

        private async Task Connect(string hostname, int port, uint timeout = 5000)
        {
            long endTime = DateTime.Now.Ticks + timeout * 10000;
            while (DateTime.Now.Ticks < endTime)
            {
                try
                {
                    Inferior = new Inferior();
                    Inferior.Attached += duktape_Attached;
                    Inferior.Detached += duktape_Detached;
                    Inferior.Throw += duktape_ErrorThrown;
                    Inferior.Print += duktape_Print;
                    Inferior.Status += duktape_Status;
                    await Inferior.Connect(hostname, port);
                    return;
                }
                catch (SocketException)
                {
                    // a SocketException in this situation just means we failed to connect,
                    // likely due to nobody listening.  we can safely ignore it; just keep trying
                    // until the timeout expires.
                }
            }
            throw new TimeoutException();
        }

        private void duktape_Attached(object sender, EventArgs e)
        {
            PluginManager.Core.Invoke(new Action(() =>
            {
                Attached?.Invoke(this, EventArgs.Empty);

                Panes.Inspector.Ssj = this;
                Panes.Errors.Ssj = this;
                Panes.Inspector.Enabled = false;
                Panes.Console.Clear();
                Panes.Errors.Clear();
                Panes.Inspector.Clear();

                Panes.Console.Print(string.Format("SSJ Blue " + plugin.Version + " Sphere JavaScript debugger"));
                Panes.Console.Print(string.Format("a graphical stepping debugger for Sphere Studio"));
                Panes.Console.Print(string.Format("(c) 2015-2016 Fat Cerberus"));
                Panes.Console.Print("");
            }), null);
        }

        private void duktape_Detached(object sender, EventArgs e)
        {
            PluginManager.Core.Invoke(new Action(async () =>
            {
                if (!expectDetach)
                {
                    await Task.Delay(1000);
                    if (!engineProcess.HasExited)
                    {
                        try
                        {
                            await Connect("localhost", 1208);
                            return;  // we're reconnected, don't detach.
                        }
                        catch (TimeoutException)
                        {
                            // if we time out on the reconnection attempt, go on and signal a
                            // detach to Sphere Studio.
                        }
                    }
                }

                focusTimer.Change(Timeout.Infinite, Timeout.Infinite);
                Detached?.Invoke(this, EventArgs.Empty);
                --plugin.Sessions;

                PluginManager.Core.Docking.Hide(Panes.Inspector);
                PluginManager.Core.Docking.Activate(Panes.Console);
                Panes.Console.Print("SSJ session has ended.");
            }), null);
        }

        private void duktape_ErrorThrown(object sender, ThrowEventArgs e)
        {
            PluginManager.Core.Invoke(new Action(() =>
            {
                Panes.Errors.Add(e.Message, e.IsFatal, e.FileName, e.LineNumber);
                PluginManager.Core.Docking.Show(Panes.Errors);
                PluginManager.Core.Docking.Activate(Panes.Errors);
                if (e.IsFatal)
                    haveError = true;
            }), null);
        }

        private void duktape_Print(object sender, TraceEventArgs e)
        {
            PluginManager.Core.Invoke(new Action(() =>
            {
                if (e.Text.StartsWith("trace: ") && !plugin.Conf.ShowTraceInfo)
                    return;
                Panes.Console.Print(e.Text);
            }), null);
        }

        private void duktape_Status(object sender, EventArgs e)
        {
            PluginManager.Core.Invoke(new Action(async () =>
            {
                bool wantPause = !Inferior.Running;
                bool wantResume = !Running && Inferior.Running;
                Running = Inferior.Running;
                if (wantPause)
                {
                    focusTimer.Change(Timeout.Infinite, Timeout.Infinite);
                    FileName = ResolvePath(Inferior.FileName);
                    LineNumber = Inferior.LineNumber;
                    if (!File.Exists(FileName))
                    {
                        // filename reported by Duktape doesn't exist; walk callstack for a
                        // JavaScript call as a fallback
                        var callStack = await Inferior.GetCallStack();
                        var topCall = callStack.First(entry => entry.Item3 != 0);
                        var callIndex = Array.IndexOf(callStack, topCall);
                        FileName = ResolvePath(topCall.Item2);
                        LineNumber = topCall.Item3;
                        await Panes.Inspector.SetCallStack(callStack, callIndex);
                        Panes.Inspector.Enabled = true;
                    }
                    else
                    {
                        updateTimer.Change(500, Timeout.Infinite);
                    }
                }
                if (wantResume && Inferior.Running)
                {
                    focusTimer.Change(250, Timeout.Infinite);
                    updateTimer.Change(Timeout.Infinite, Timeout.Infinite);
                    Panes.Errors.ClearHighlight();
                }
                if (wantPause && Paused != null)
                {
                    PauseReason reason = haveError ? PauseReason.Exception : PauseReason.Breakpoint;
                    haveError = false;
                    Paused(this, new PausedEventArgs(reason));
                }
                if (wantResume && Resumed != null)
                    Resumed(this, EventArgs.Empty);
            }), null);
        }

        public async Task SetBreakpoint(string fileName, int lineNumber)
        {
            fileName = UnresolvePath(fileName);
            await Inferior.AddBreak(fileName, lineNumber);
        }

        public async Task ClearBreakpoint(string fileName, int lineNumber)
        {
            fileName = UnresolvePath(fileName);

            // clear all matching breakpoints
            var breaks = await Inferior.ListBreak();
            for (int i = breaks.Length - 1; i >= 0; --i)
            {
                string fn = breaks[i].Item1;
                int line = breaks[i].Item2;
                if (fileName == fn && lineNumber == line)
                    await Inferior.DelBreak(i);
            }

        }

        public async Task Resume()
        {
            await Inferior.Resume();
        }

        public async Task Pause()
        {
            await Inferior.Pause();
        }

        public async Task StepInto()
        {
            await Inferior.StepInto();
        }

        public async Task StepOut()
        {
            await Inferior.StepOut();
        }

        public async Task StepOver()
        {
            await Inferior.StepOver();
        }

        private static void HandleFocusSwitch(object state)
        {
            PluginManager.Core.Invoke(new Action(() =>
            {
                SsjDebugger me = (SsjDebugger)state;
                try
                {
                    NativeMethods.SetForegroundWindow(me.engineProcess.MainWindowHandle);
                    Panes.Inspector.Enabled = false;
                    Panes.Inspector.Clear();
                }
                catch (InvalidOperationException)
                {
                    // this will be thrown by Process.MainWindowHandle if the process
                    // is no longer running; we can safely ignore it.
                }
            }), null);
        }

        private static void UpdateDebugViews(object state)
        {
            PluginManager.Core.Invoke(new Action(async () =>
            {
                SsjDebugger me = (SsjDebugger)state;
                var callStack = await me.Inferior.GetCallStack();
                if (!me.Running)
                {
                    await Panes.Inspector.SetCallStack(callStack);
                    Panes.Inspector.Enabled = true;
                    PluginManager.Core.Docking.Activate(Panes.Inspector);
                }
            }), null);
        }

        /// <summary>
        /// Resolves a SphereFS path into an absolute one.
        /// </summary>
        /// <param name="path">The SphereFS path to resolve.</param>
        internal string ResolvePath(string path)
        {
            if (Path.IsPathRooted(path))
                return path.Replace('/', Path.DirectorySeparatorChar);
            if (path.StartsWith("@/"))
                path = Path.Combine(sourcePath, path.Substring(2));
            else if (path.StartsWith("#/"))
                path = Path.Combine(engineDir, "system", path.Substring(2));
            else if (path.StartsWith("~/"))
                path = Path.Combine(
                    Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments),
                    "minisphere", path.Substring(2));
            else
                path = Path.Combine(sourcePath, path);
            return path.Replace('/', Path.DirectorySeparatorChar);
        }

        /// <summary>
        /// Converts an absolute path into a SphereFS path. If this is
        /// not possible, leaves the path as-is.
        /// </summary>
        /// <param name="path">The absolute path to unresolve.</param>
        private string UnresolvePath(string path)
        {
            var pathSep = Path.DirectorySeparatorChar.ToString();
            string sourceRoot = sourcePath.EndsWith(pathSep)
                ? sourcePath : sourcePath + pathSep;
            string sysRoot = Path.Combine(engineDir, @"system") + pathSep;

            if (path.StartsWith(sysRoot))
                path = string.Format("#/{0}", path.Substring(sysRoot.Length).Replace(pathSep, "/"));
            else if (path.StartsWith(sourceRoot))
                path = path.Substring(sourceRoot.Length).Replace(pathSep, "/");
            return path;
        }

        private void UpdateStatus()
        {
            FileName = ResolvePath(Inferior.FileName);
            LineNumber = Inferior.LineNumber;
            Running = Inferior.Running;
        }
    }
}
