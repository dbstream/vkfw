#!/usr/bin/env python
# Generate core/vk_functions.cc and include/VKFW/vk_functions.h
# Copyright (C) 2024  dbstream
#
# Based on generate.py from https://github.com/zeux/volk at commit f51cfb6.
#
# This file is licensed under the MIT license.
import re
import sys
import xml.etree.ElementTree as etree
from collections import OrderedDict

# Generate vk_functions.cc and vk_functions.h from vk.xml.

cmdversions = {
	'vkCmdSetDiscardRectangleEnableEXT': 2,
	'vkCmdSetDiscardRectangleModeEXT': 2,
	'vkCmdSetExclusiveScissorEnableNV': 2
}

def is_descendant (types, name, base):
	if name == base:
		return True
	typ = types.get (name)
	if not typ:
		return False
	parents = typ.get ('parent')
	if not parents:
		return False
	return any ([is_descendant (types, parent, base) for parent in parents.split (',')])

def cdepends (key):
	key = re.sub (r'[a-zA-Z0-9_]+', lambda m: 'defined(' + m.group(0) + ')', key)
	key = key.replace (',', ' || ')
	key = key.replace ('+', ' && ')
	return key

def patch_file (path, blocks):
	result = []
	block = None

	with open (path, 'r') as filp:
		for line in filp.readlines ():
			if block:
				if line.strip () == block.strip ():
					result.append (line)
					block = None
			else:
				result.append (line)
				if line.strip ().startswith ('/* VKFW_GEN_'):
					block = line
					result.append (blocks[line.strip ()[12:-3]])
	with open (path, 'w', newline='\n') as filp:
		for line in result:
			filp.write (line)

def main (argv):
	spec_path = '/usr/share/vulkan/registry/vk.xml'
	if len (argv) > 1:
		spec_path = argv[1]

	with open (spec_path, 'r') as fd:
		spec = etree.parse (fd)

	command_groups = OrderedDict ()

	for feature in spec.findall ('feature'):
		if 'vulkan' not in feature.get ('api').split (','):
			continue
		key = 'defined(' + feature.get ('name') + ')'
		cmdrefs = feature.findall ('require/command')
		command_groups[key] = [cmdref.get ('name') for cmdref in cmdrefs]

	instance_commands = set ()

	for ext in sorted (spec.findall ('extensions/extension'), key=lambda ext: ext.get ('name')):
		if 'vulkan' not in ext.get ('supported').split (','):
			continue
		name = ext.get ('name')
		typ = ext.get ('type')
		for req in ext.findall ('require'):
			key = 'defined(' + name + ')'
			if req.get ('feature'):
				for i in req.get ('feature').split (','):
					key += ' && defined(' + i + ')'
			if req.get ('extension'):
				for i in req.get ('extension').split (','):
					key += ' && defined(' + i + ')'
			if req.get ('depends'):
				dep = cdepends (req.get ('depends'))
				if '||' in dep:
					key += ' && (' + dep + ')'
				else:
					key += ' && ' + dep
			cmdrefs = req.findall ('command')
			for cmdref in cmdrefs:
				ver = cmdversions.get (cmdref.get ('name'))
				real_key = key
				if ver:
					real_key += ' && ' + name.upper() + '_SPEC_VERSION >= ' + str (ver)
				command_groups.setdefault (real_key, []).append (cmdref.get ('name'))
			if typ == 'instance':
				for cmdref in cmdrefs:
					instance_commands.add (cmdref.get ('name'))

	command_to_groups = OrderedDict ()

	for (group, cmdnames) in command_groups.items ():
		for name in cmdnames:
			command_to_groups.setdefault (name, []).append (group)

	for (group, cmdnames) in command_groups.items ():
		command_groups[group] = [name for name in cmdnames if len (command_to_groups[name]) == 1]

	for (name, groups) in command_to_groups.items ():
		if len (groups) == 1:
			continue
		key = ' || '.join (['(' + group + ')' for group in groups])
		command_groups.setdefault (key, []).append (name)

	commands = {}

	for cmd in spec.findall ('commands/command'):
		if not cmd.get ('alias'):
			name = cmd.findtext ('proto/name')
			commands[name] = cmd

	for cmd in spec.findall ('commands/command'):
		if cmd.get ('alias'):
			name = cmd.get ('name')
			commands[name] = commands[cmd.get ('alias')]

	types = {}

	for typ in spec.findall ('types/type'):
		name = typ.findtext ('name')
		if name:
			types[name] = typ

	blocks = {
		'PROTOTYPES_H': '',
		'PROTOTYPES_C': '',
		'PFNS': '',
		'LOAD_LOADER': '',
		'LOAD_INSTANCE': '',
		'LOAD_DEVICE': ''
	}

	for (group, cmdnames) in command_groups.items ():
		ifdef = '#if ' + group + '\n'
		for key in blocks.keys ():
			blocks[key] += ifdef

		for name in sorted (cmdnames):
			cmd = commands[name]
			proto = cmd.findtext ('proto/type')
			params = cmd.findall ('param')
			params = [p for p in params if (not p.get ('api')) or 'vulkan' in p.get ('api').split (',')]
			param_names = [param.findtext ('name') for param in params]
			typ = params[0].findtext ('type')

			if name == 'vkGetDeviceProcAddr':
				typ = 'VkInstance'		

			decl = 'VKFWAPI VKAPI_ATTR ' + proto + ' VKAPI_CALL\n' + name + ' (' + ', '.join ([' '.join (param.itertext ()) for param in params]) + ')'
			blocks['PROTOTYPES_H'] += decl + ';' + '\n'
			blocks['PROTOTYPES_C'] += 'extern "C"\n' + decl + '\n{\n\t' + ('return ' if proto != 'void' else '') + 'pfn_' + name + ' (' + ', '.join(param_names) + ');\n}\n'

			blocks['PFNS'] += 'PFN_' + name + ' pfn_' + name + ';\n'

			if is_descendant (types, typ, 'VkDevice') and name not in instance_commands:
				blocks['LOAD_DEVICE'] += '\tpfn_' + name + ' = (PFN_' + name + ') load (context, "' + name + '");\n'
			elif is_descendant (types, typ, 'VkInstance'):
				blocks['LOAD_INSTANCE'] += '\tpfn_' + name + ' = (PFN_' + name + ') load (context, "' + name + '");\n'
			elif name != 'vkGetInstanceProcAddr':
				blocks['LOAD_LOADER'] += '\tpfn_' + name + ' = (PFN_' + name + ') load (context, "' + name + '");\n'

		for key in blocks.keys ():
			if blocks[key].endswith (ifdef):
				blocks[key] = blocks[key][:-len(ifdef)]
			else:
				blocks[key] += '#endif /* ' + group + ' */\n'

	patch_file ('core/vk_functions.cc', blocks)
	patch_file ('include/VKFW/vk_functions.h', blocks)

if __name__ == '__main__':
	main (sys.argv)
