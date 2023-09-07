const { argv, env, exit, platform, stdin, stdout, stderr } = require('process');

try {
	require('uglifycss/package');
} catch {
	const { spawnSync } = require('child_process');
	let result = null;
	if (platform == 'win32') {
		result = spawnSync('cmd', ['/C', 'npm', '--prefix=' + env.NODE_PREFIX, 'install', 'uglifycss'], {'stdio': ['ignore', 'ignore', 'inherit']});
	} else {
		result = spawnSync('npm', ['--prefix=' + env.NODE_PREFIX, 'install', 'uglifycss'], {'stdio': ['ignore', 'ignore', 'inherit']});
	}
	if (result.status != 0) {
		stderr.write("npm failed to run, is it installed?\n");
		exit(result.status);
	}
	result = spawnSync('node', argv.slice(1), {'stdio': 'inherit'});
	exit(result.status);
}

const uglifycss = require('uglifycss');

var input = '';
stdin.setEncoding('utf-8');
stdin.on('data', (data) => {
	input = input.concat(data.toString());
});
stdin.on('close', () => {
	let output = uglifycss.processString(input);
	stdout.write(output);
	exit(0);
});