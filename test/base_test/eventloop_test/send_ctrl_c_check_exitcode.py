# 向本机控制台子进程发送 Ctrl+C，并打印其退出码（用于验证 Windows 上优雅退出为 0）。
# 用法：在含已编译 eventloop_simple_test.exe 的目录下，或设置环境变量 EL_SIMPLE_EXE 为 exe 绝对路径。
import ctypes
import os
import subprocess
import sys
import time
from ctypes import wintypes

kernel32 = ctypes.windll.kernel32
PHANDLER_ROUTINE = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.DWORD)


@PHANDLER_ROUTINE
def _parent_ignore_ctrl(_ctrl_type: int) -> bool:
    return True


def main() -> int:
    exe = os.environ.get("EL_SIMPLE_EXE")
    if not exe:
        here = os.path.dirname(os.path.abspath(__file__))
        cand = os.path.normpath(
            os.path.join(here, "..", "..", "..", "build", "bin", "Debug", "eventloop_simple_test.exe")
        )
        exe = cand if os.path.isfile(cand) else None
    if not exe or not os.path.isfile(exe):
        print("Set EL_SIMPLE_EXE to eventloop_simple_test.exe path", file=sys.stderr)
        return 2

    kernel32.SetConsoleCtrlHandler(_parent_ignore_ctrl, True)
    p = subprocess.Popen([exe])
    time.sleep(2)
    if not kernel32.GenerateConsoleCtrlEvent(0, 0):
        print("GenerateConsoleCtrlEvent failed", kernel32.GetLastError(), file=sys.stderr)
        p.kill()
        return 2
    code = p.wait(timeout=30)
    print("child_exit_code:", code)
    return 0 if code == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
