"""Runs a command on a desktop of its own: its windows can take neither focus nor input.

Never switches to that desktop. It goes away with its last handle. Nobody sees a dialog there, so
the command and all it started end after a time limit: run_hidden.py [--timeout seconds] command
"""
import ctypes
import sys
from ctypes import wintypes as w

k = ctypes.WinDLL('kernel32', use_last_error=True)
u = ctypes.WinDLL('user32', use_last_error=True)


class SI(ctypes.Structure):
    _fields_ = [('cb', w.DWORD), ('lpReserved', w.LPWSTR), ('lpDesktop', w.LPWSTR), ('lpTitle', w.LPWSTR),
                ('dwX', w.DWORD), ('dwY', w.DWORD), ('dwXSize', w.DWORD), ('dwYSize', w.DWORD),
                ('dwXCountChars', w.DWORD), ('dwYCountChars', w.DWORD), ('dwFillAttribute', w.DWORD),
                ('dwFlags', w.DWORD), ('wShowWindow', w.WORD), ('cbReserved2', w.WORD),
                ('lpReserved2', ctypes.c_void_p), ('hStdInput', w.HANDLE), ('hStdOutput', w.HANDLE),
                ('hStdError', w.HANDLE)]


class PI(ctypes.Structure):
    _fields_ = [('hProcess', w.HANDLE), ('hThread', w.HANDLE), ('dwProcessId', w.DWORD), ('dwThreadId', w.DWORD)]


u.CreateDesktopW.restype = w.HANDLE
u.CreateDesktopW.argtypes = [w.LPCWSTR, w.LPCWSTR, ctypes.c_void_p, w.DWORD, w.DWORD, ctypes.c_void_p]
k.CreateProcessW.argtypes = [w.LPCWSTR, w.LPWSTR, ctypes.c_void_p, ctypes.c_void_p, w.BOOL, w.DWORD,
                             ctypes.c_void_p, w.LPCWSTR, ctypes.POINTER(SI), ctypes.POINTER(PI)]
k.WaitForSingleObject.argtypes = [w.HANDLE, w.DWORD]
k.GetExitCodeProcess.argtypes = [w.HANDLE, ctypes.POINTER(w.DWORD)]

class Limits(ctypes.Structure):
    """JOBOBJECT_EXTENDED_LIMIT_INFORMATION, of which only LimitFlags matters."""

    _fields_ = [('times', ctypes.c_longlong * 2), ('LimitFlags', w.DWORD), ('sizes', ctypes.c_size_t * 2),
                ('ActiveProcessLimit', w.DWORD), ('Affinity', ctypes.c_size_t), ('PriorityClass', w.DWORD),
                ('SchedulingClass', w.DWORD), ('io', ctypes.c_ulonglong * 6), ('memory', ctypes.c_size_t * 4)]


k.CreateJobObjectW.restype = w.HANDLE
k.CreateJobObjectW.argtypes = [ctypes.c_void_p, w.LPCWSTR]
k.SetInformationJobObject.argtypes = [w.HANDLE, ctypes.c_int, ctypes.c_void_p, w.DWORD]
k.AssignProcessToJobObject.argtypes = [w.HANDLE, w.HANDLE]
k.ResumeThread.argtypes = [w.HANDLE]
k.TerminateJobObject.argtypes = [w.HANDLE, w.UINT]

arguments = sys.argv[1:]
timeout_seconds = 1800
if arguments[:1] == ['--timeout']:
    timeout_seconds = int(arguments[1])
    arguments = arguments[2:]

name = 'qtd_tests'
desktop = u.CreateDesktopW(name, None, None, 0, 0x10000000, None)  # GENERIC_ALL
if not desktop:
    sys.exit(f'CreateDesktop failed: {ctypes.get_last_error()}')
si = SI(cb=ctypes.sizeof(SI), lpDesktop=name)
pi = PI()
line = ctypes.create_unicode_buffer(' '.join(arguments))
CREATE_NO_WINDOW = 0x08000000
CREATE_SUSPENDED = 0x00000004
if not k.CreateProcessW(None, line, None, None, False, CREATE_NO_WINDOW | CREATE_SUSPENDED, None, None,
                        ctypes.byref(si), ctypes.byref(pi)):
    sys.exit(f'CreateProcess failed: {ctypes.get_last_error()}')
job = k.CreateJobObjectW(None, None)
limits = Limits(LimitFlags=0x2000)  # JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE
k.SetInformationJobObject(job, 9, ctypes.byref(limits), ctypes.sizeof(limits))  # extended limits
k.AssignProcessToJobObject(job, pi.hProcess)
k.ResumeThread(pi.hThread)
if k.WaitForSingleObject(pi.hProcess, timeout_seconds * 1000) != 0:
    k.TerminateJobObject(job, 1)
    sys.exit(f'timed out after {timeout_seconds} s')
code = w.DWORD()
k.GetExitCodeProcess(pi.hProcess, ctypes.byref(code))
sys.exit(code.value)
