"""Strict PID + creation-time exit checks; access errors are never an exit."""
import ctypes
from ctypes import wintypes
import json
import subprocess
import shutil
from pathlib import Path
import os
from datetime import datetime,timezone
from run import ROOT,TASK,write
def now():return datetime.now(timezone.utc).isoformat()

def inspect(expected):
    kernel=ctypes.WinDLL('kernel32',use_last_error=True)
    kernel.OpenProcess.argtypes=[wintypes.DWORD,wintypes.BOOL,wintypes.DWORD]
    kernel.OpenProcess.restype=wintypes.HANDLE
    kernel.GetProcessTimes.argtypes=[wintypes.HANDLE]+[ctypes.POINTER(wintypes.FILETIME)]*4
    kernel.WaitForSingleObject.argtypes=[wintypes.HANDLE,wintypes.DWORD]
    kernel.CloseHandle.argtypes=[wintypes.HANDLE]
    handle=kernel.OpenProcess(0x00101000,False,expected['pid'])
    if not handle:
        error=ctypes.get_last_error()
        if error == 5: return prove_reused_with_cim(expected)
        if error!=87:raise ctypes.WinError(error)
        return {'expected':expected,'status':'EXITED_ABSENT'}
    try:
        times=[wintypes.FILETIME() for _ in range(4)]
        if not kernel.GetProcessTimes(handle,*(ctypes.byref(t) for t in times)):
            raise ctypes.WinError(ctypes.get_last_error())
        created=(times[0].dwHighDateTime<<32)+times[0].dwLowDateTime
        if created!=expected['creation_filetime']:
            return {'expected':expected,'status':'EXITED_PID_REUSED','current_creation_filetime':created}
        wait=kernel.WaitForSingleObject(handle,0)
        if wait==0:return {'expected':expected,'status':'EXITED_HANDLE_SIGNALED'}
        if wait!=258:raise ctypes.WinError(ctypes.get_last_error())
        return {'expected':expected,'status':'ALIVE'}
    finally:kernel.CloseHandle(handle)

def verified_reuse(expected, rows):
    if len(rows) != 1: raise RuntimeError('CIM must return one identity after access denial')
    current = rows[0]
    if int(current['ProcessId']) != expected['pid']: raise RuntimeError('CIM PID mismatch')
    created = int(current['creation_filetime'])
    if created // 10 <= expected['creation_filetime'] // 10:
        raise RuntimeError('CIM did not prove later unrelated creation; access error is not exit')
    return {'expected':expected, 'status':'EXITED_PID_REUSED_CIM_VERIFIED',
            'current_creation_filetime':created, 'current_name':current['Name'],
            'initial_handle_query':{'result':'FAIL','winerror':5}, 'checked_utc':now(),
            'evidence':'Successful Win32_Process CreationDate read, strictly later microsecond'}

def prove_reused_with_cim(expected):
    command = ("$ErrorActionPreference='Stop'; $rows=@(Get-CimInstance Win32_Process "
        f"-Filter 'ProcessId = {int(expected['pid'])}' -ErrorAction Stop | "
        "Select-Object ProcessId,Name,@{n='creation_filetime';"
        "e={$_.CreationDate.ToUniversalTime().ToFileTimeUtc().ToString()}}); "
        "ConvertTo-Json -InputObject $rows -Compress")
    response = subprocess.run([shutil.which('pwsh') or 'powershell','-NoProfile','-NonInteractive',
        '-Command',command], capture_output=True, text=True, timeout=30, check=True)
    if response.stderr.strip(): raise RuntimeError('CIM error: '+response.stderr)
    return verified_reuse(expected,json.loads(response.stdout))

def self_check():
    expected={'pid':123,'creation_filetime':1009}
    good={'ProcessId':123,'creation_filetime':'1010','Name':'example'}
    assert verified_reuse(expected,[good])['status']=='EXITED_PID_REUSED_CIM_VERIFIED'
    for rows in ([],[good,good],[dict(good,ProcessId=124)],
                 [dict(good,creation_filetime='1000')],[dict(good,creation_filetime='999')]):
        try: verified_reuse(expected,rows)
        except RuntimeError: continue
        raise AssertionError('Unproven exit accepted')


def main():
    self_check();rows={};events={};seen=set();results={}
    while True:
        files=list((TASK/'journal').glob('*.json'))
        for path in files:
            d=json.loads(path.read_text());key=(d['pid'],d['creation_filetime'])
            rows[key]=d;events.setdefault(key,set()).add(path.stem.rsplit('-',1)[-1])
        pending=[k for k in rows if k not in seen and k[0]!=os.getpid()]
        if not pending:break
        for k in pending:
            results[k]=inspect(rows[k]);seen.add(k)
            if results[k]['status']=='ALIVE':raise RuntimeError('Owned process still alive: '+str(rows[k]))
    for k,ev in events.items():
        if 'spawn' in ev and 'exit' not in ev:raise RuntimeError('Retained child exit event missing: '+str(k))
    result={'result':'PASS','checked_utc':now(),'identities_exited':len(results),'processes':list(results.values()),'retained_children':sum('spawn' in ev for ev in events.values()),'auditor_self':[v for k,v in rows.items() if k[0]==os.getpid()], 'auditor_exit':'PENDING outer handle wait; not counted as exited here', 'compiler_descendants':'CMake/ninja/compiler grandchildren were synchronously waited through CMake, not individually journalled; direct CMake handles and exit codes are preserved.'}
    write(TASK/'process-exit-audit.json',result)
    print(json.dumps({k:v for k,v in result.items() if k!='processes'}))
if __name__=='__main__':main()
