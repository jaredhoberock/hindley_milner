env = Environment(CCFLAGS = "-std=c++0x -Wall -g")

if env['PLATFORM'] == 'darwin':
  env['CXX'] = '/opt/local/bin/g++-mp-4.5'

sources = Glob('*.cpp')

env.Program('demo', "demo.cpp")

