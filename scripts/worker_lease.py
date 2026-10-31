"""Temporarily drain the existing local T8 worker using its exit-file contract.

No remote-worker API is called. Only the pinned launcher's existing PID is
waited for; restarting uses that same launcher and its single-instance check.
"""
import ctypes
from ctypes import wintypes
import json
import os
from pathlib import Path
import subprocess
import sys
import time
import uuid

LAUNCHER=Path(r'D:\OpenBench-workers\launch_t8.py')
CLIENT=Path(r'D:\OpenBench-workers\opportunistic-t8-20260719-launch2\Client')

class WorkerLease:
    def __init__(self,report):
        self.report=report
        self.token='petrified-'+str(uuid.uuid4())
        self.flag=CLIENT/'openbench.exit'
        self.owned=False
        self.handle=None
        self.events=[]

    def record(self,event,**fields):
        self.events.append(dict(event=event,utc=time.time(),**fields))
        self.report.write_text(json.dumps(self.events,indent=2)+'\n')

    def __enter__(self):
        if os.name!='nt': raise RuntimeError('This worker lease is Windows-specific')
        pid=int((CLIENT/'t8-openbench.pid').read_text())
        # Query only identity predicates, never emit a command line or secrets.
        predicate=f"$p=Get-CimInstance Win32_Process -Filter 'ProcessId = {pid}'; if ($p.Name -match '^python' -and $p.CommandLine -match 'client\\.py' -and $p.CommandLine -match 'belzedar' -and $p.CommandLine -match '-T 8(?: |$)') {{ $p.ExecutablePath }}"
        result=subprocess.check_output(['powershell','-NoProfile','-Command',predicate],text=True)
        self.worker_python=Path(result.strip())
        if not result.strip() or not self.worker_python.is_file():
            raise RuntimeError('Recorded local T8 worker is not running; refusing to invent an instance')
        kernel=ctypes.WinDLL('kernel32',use_last_error=True)
        kernel.OpenProcess.argtypes=[wintypes.DWORD,wintypes.BOOL,wintypes.DWORD]
        kernel.OpenProcess.restype=wintypes.HANDLE
        kernel.WaitForSingleObject.argtypes=[wintypes.HANDLE,wintypes.DWORD]
        kernel.CloseHandle.argtypes=[wintypes.HANDLE]
        self.kernel=kernel
        self.handle=kernel.OpenProcess(0x00100000,False,pid)
        if not self.handle: raise ctypes.WinError(ctypes.get_last_error())
        try:
            with self.flag.open('x') as f: f.write(self.token)
            self.owned=True
            self.record('pause-requested',pid=pid)
            deadline=time.monotonic()+600
            while kernel.WaitForSingleObject(self.handle,1000)==258:
                if time.monotonic()>deadline: raise TimeoutError('Local worker did not drain within ten minutes')
            self.record('paused',pid=pid)
            return self
        except BaseException:
            self.restore()
            raise

    def restore(self):
        if self.owned:
            # A cooperative cancellation during draining may race the client's
            # abort flag. Wait for that exact old process before restoring it.
            deadline=time.monotonic()+600
            while self.kernel.WaitForSingleObject(self.handle,1000)==258:
                if time.monotonic()>deadline:
                    raise TimeoutError('Old T8 client has not exited; refusing a duplicate restart')
            if not self.flag.exists() or self.flag.read_text()!=self.token:
                raise RuntimeError('Worker exit-file ownership changed; refusing to overwrite it')
            self.flag.unlink()
            # The launcher checks for an existing client, so a pause timeout
            # cannot start a duplicate. Keep launcher output for auditing.
            result=subprocess.run([str(self.worker_python),str(LAUNCHER)],capture_output=True,text=True,timeout=60)
            self.record('restore-t8',returncode=result.returncode,output=result.stdout.strip())
            time.sleep(3)
            status=subprocess.check_output([str(self.worker_python),str(LAUNCHER),'--estado'],text=True,timeout=30)
            if 'VIVO' not in status: raise RuntimeError('T8 restoration could not be verified: '+status)
            self.record('restored',status=status.strip())
            self.owned=False
        if self.handle:
            self.kernel.CloseHandle(self.handle); self.handle=None

    def __exit__(self,*exc): self.restore()
