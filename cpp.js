#!/usr/bin/env node
require('coffeescript/register')
const cpp = require('./cpp.coffee')
const path = require('path')

var options = require('command-line-args')([
	{ name: 'src', type: String, defaultOption: true },
	{ name: 'namespace', alias: 'n', type: String, defaultValue: '' },
	{ name: 'out', alias: 'o', type: String, defaultValue: '' }])

var resultSource = cpp.generate(require(path.resolve(options.src)))

if (options.out)
	require('fs').writeFileSync(options.out, resultSource)
else
	process.stdout.write(resultSource)
