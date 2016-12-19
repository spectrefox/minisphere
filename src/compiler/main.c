#include "cell.h"

#include "build.h"

static bool parse_cmdline    (int argc, char* argv[]);
static void print_cell_quote (void);
static void print_banner     (bool want_copyright, bool want_deps);
static void print_usage      (void);

static path_t* s_in_path;
static path_t* s_out_path;
bool           s_want_source_map;
static char*   s_target_name;

int
main(int argc, char* argv[])
{
	int      retval = EXIT_FAILURE;

	srand((unsigned int)time(NULL));

	// parse the command line
	if (!parse_cmdline(argc, argv))
		goto shutdown;

	print_banner(true, false);
	printf("\n");

	retval = EXIT_SUCCESS;

shutdown:
	path_free(s_in_path);
	path_free(s_out_path);
	free(s_target_name);
	return retval;
}

static bool
parse_cmdline(int argc, char* argv[])
{
	path_t*     cellscript_path;
	int         num_targets = 0;
	const char* short_args;

	int    i;
	size_t i_arg;

	// establish default options
	s_in_path = path_new("./");
	s_out_path = NULL;
	s_target_name = strdup("default");
	s_want_source_map = false;
	
	// validate and parse the command line
	for (i = 1; i < argc; ++i) {
		if (strstr(argv[i], "--") == argv[i]) {
			if (strcmp(argv[i], "--help") == 0) {
				print_usage();
				return false;
			}
			else if (strcmp(argv[i], "--version") == 0) {
				print_banner(true, true);
				return false;
			}
			else if (strcmp(argv[i], "--in-dir") == 0) {
				if (++i >= argc) goto missing_argument;
				path_free(s_in_path);
				s_in_path = path_new_dir(argv[i]);
			}
			else if (strcmp(argv[i], "--explode") == 0) {
				print_cell_quote();
				return false;
			}
			else if (strcmp(argv[i], "--package") == 0) {
				if (++i >= argc) goto missing_argument;
				if (s_out_path != NULL) {
					printf("cell: too many outputs\n");
					return false;
				}
				s_out_path = path_new(argv[i]);
				if (path_filename_cstr(s_out_path) == NULL) {
					printf("cell: `%s` argument cannot be a directory\n", argv[i - 1]);
					return false;
				}
			}
			else if (strcmp(argv[i], "--build") == 0) {
				if (++i >= argc) goto missing_argument;
				if (s_out_path != NULL) {
					printf("cell: too many outputs\n");
					return false;
				}
				s_out_path = path_new_dir(argv[i]);
			}
			else if (strcmp(argv[i], "--debug") == 0) {
				s_want_source_map = true;
			}
			else {
				printf("cell: unknown option `%s`\n", argv[i]);
				return false;
			}
		}
		else if (argv[i][0] == '-') {
			short_args = argv[i];
			for (i_arg = strlen(short_args) - 1; i_arg >= 1; --i_arg) {
				switch (short_args[i_arg]) {
				case 'b':
					if (++i >= argc) goto missing_argument;
					if (s_out_path != NULL) {
						printf("cell: too many outputs\n");
						return false;
					}
					s_out_path = path_new_dir(argv[i]);
					break;
				case 'p':
					if (++i >= argc) goto missing_argument;
					if (s_out_path != NULL) {
						printf("cell: too many outputs\n");
						return false;
					}
					s_out_path = path_new(argv[i]);
					if (path_filename_cstr(s_out_path) == NULL) {
						printf("cell: `%s` argument cannot be a directory\n", short_args);
						return false;
					}
					break;
				case 'd':
					s_want_source_map = true;
					break;
				default:
					printf("cell: unknown option `-%c`\n", short_args[i_arg]);
					return false;
				}
			}
		}
		else {
			free(s_target_name);
			s_target_name = strdup(argv[i]);
		}
	}
	
	// validate command line
	if (s_out_path == NULL) {
		print_usage();
		return false;
	}
	
	// check if a Cellscript exists
	cellscript_path = path_rebase(path_new("Cellscript.js"), s_in_path);
	if (!path_resolve(cellscript_path, NULL)) {
		path_free(cellscript_path);
		printf("no Cellscript.js found in source directory\n");
		return false;
	}
	
	path_free(cellscript_path);
	return true;

missing_argument:
	printf("cell: `%s` requires an argument\n", argv[i - 1]);
	return false;
}

static void
print_cell_quote(void)
{
	// yes, these are entirely necessary. :o)
	static const char* const MESSAGES[] =
	{
		"This is the end for you!",
		"Very soon, I am going to explode. And when I do...",
		"Careful now! I wouldn't attack me if I were you...",
		"I'm quite volatile, and the slightest jolt could set me off!",
		"One minute left! There's nothing you can do now!",
		"If only you'd finished me off a little bit sooner...",
		"Ten more seconds, and the Earth will be gone!",
		"Let's just call our little match a draw, shall we?",
		"Watch out! You might make me explode!",
	};

	printf("Cell seems to be going through some sort of transformation...\n");
	printf("He's pumping himself up like a balloon!\n\n");
	printf("    Cell says:\n    \"%s\"\n", MESSAGES[rand() % (sizeof MESSAGES / sizeof(const char*))]);
}

static void
print_banner(bool want_copyright, bool want_deps)
{
	printf("%s %s Sphere packaging compiler (%s)\n", COMPILER_NAME, VERSION_NAME,
		sizeof(void*) == 8 ? "x64" : "x86");
	if (want_copyright) {
		printf("a programmable build engine for your Sphere games\n");
		printf("(c) 2015-2016 Fat Cerberus\n");
	}
	if (want_deps) {
		printf("\n");
		printf("   Duktape: %s\n", DUK_GIT_DESCRIBE);
		printf("      zlib: v%s\n", zlibVersion());
	}
}

static void
print_usage(void)
{
	print_banner(true, false);
	printf("\n");
	printf("USAGE:\n");
	printf("   cell -b <out-dir> [options] [target]\n");
	printf("   cell -p <out-file> [options] [target]\n");
	printf("\n");
	printf("OPTIONS:\n");
	printf("       --in-dir    Set the input directory. Without this option, Cell will   \n");
	printf("                   look for sources in the current working directory.        \n");
	printf("   -b, --build     Build an unpackaged Sphere game distribution.             \n");
	printf("   -p, --package   Build a Sphere package (.spk).                            \n");
	printf("   -d, --debug     Include a debugging map in the compiled game which maps   \n");
	printf("                   compiled assets to their corresponding source files.      \n");
	printf("       --version   Print the version number of Cell and its dependencies.    \n");
	printf("       --help      Print this help text.                                     \n");
}
