/**
 *  miniRT test CommonJS module
 *  (c) 2015-2016 Fat Cerberus
**/

'use strict';
module.exports =
{
	run: run
};

const link = require('link');

function run(tests)
{
	link.Q(Object.keys(tests))
		.where(function(key) { return key.substring(0, 4) === 'test'; })
		.where(function(key) { return key !== 'test' })
		.map(function(key) { return tests[key]; })
		.forEach(function(test)
	{
		if (typeof test === 'function')
			test();
		else if (typeof test === 'object' && test !== null)
			run(test);
	});
}
