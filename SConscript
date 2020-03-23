from building import *
Import('rtconfig')

src   = []
cwd   = GetCurrentDir()

# add src files.
if GetDepend('PKG_USING_TASK_MSG_BUS'):
    src += Glob('src/task_msg_bus.c')

if GetDepend('PKG_USING_TASK_MSG_BUS_SAMPLE'):
    src += Glob('examples/task_msg_bus_sample.c')

# add include path.
path  = [cwd + '/inc']

# add src and include to group.
group = DefineGroup('task_msg_bus', src, depend = ['PKG_USING_TASK_MSG_BUS'], CPPPATH = path)

Return('group')
