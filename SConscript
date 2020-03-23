from building import *
Import('rtconfig')

src = []
path = []
cwd   = GetCurrentDir()

if GetDepend('PKG_USING_TASK_MSG_BUS'):
    src += Glob('src/task_msg_bus.c')
    path += [cwd + '/inc']

if GetDepend('PKG_USING_TASK_MSG_BUS_SAMPLE'):
    src += Glob('examples/task_msg_bus_sample.c')
    path += [cwd + '/examples']

# add src and include to group.
group = DefineGroup('task_msg_bus', src, depend = ['PKG_USING_TASK_MSG_BUS'], CPPPATH = path)

Return('group')
