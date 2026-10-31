$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$task = Join-Path $root '.local/pr18-diagnosis-v1'
$leasePath = Join-Path $root '.local/petrified-machine-lease.json'
function Read-Json([string]$path) { Get-Content -LiteralPath $path -Raw | ConvertFrom-Json }
function Write-Json([string]$path, $value) {
    $tmp = $path + '.tmp'
    [IO.File]::WriteAllText($tmp, ($value | ConvertTo-Json -Depth 30) + "`n", [Text.UTF8Encoding]::new($false))
    Move-Item -LiteralPath $tmp -Destination $path -Force
}
$archive = Read-Json (Join-Path $task 'archive-verification.json')
$verified = Read-Json (Join-Path $task 'verification.json')
$completed = Read-Json (Join-Path $task 'complete.json')
$exitAudit = Read-Json (Join-Path $task 'process-exit-audit.json')
foreach ($report in @($archive,$verified,$completed,$exitAudit)) {
    if ($report.result -ne 'PASS') { throw 'Required diagnostic evidence did not pass' }
}
if ((Get-FileHash -LiteralPath (Join-Path $root $archive.archive) -Algorithm SHA256).Hash.ToLowerInvariant() -ne $archive.sha256) { throw 'Archive changed' }
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class DiagnosisProcessProof {
 [DllImport("kernel32.dll", SetLastError=true)] public static extern IntPtr OpenProcess(uint access, bool inherit, uint pid);
 [DllImport("kernel32.dll", SetLastError=true)] public static extern bool GetProcessTimes(IntPtr h, out long created, out long exited, out long kernel, out long user);
 [DllImport("kernel32.dll", SetLastError=true)] public static extern uint WaitForSingleObject(IntPtr h, uint timeout);
 [DllImport("kernel32.dll")] public static extern bool CloseHandle(IntPtr h);
}
'@
$identities = @{}
$events = @{}
foreach ($file in Get-ChildItem -LiteralPath (Join-Path $task 'journal') -Filter '*.json') {
    $d = Read-Json $file.FullName
    $key = "$( $d.pid )-$( $d.creation_filetime )"
    $identities[$key] = $d
    if (-not $events.ContainsKey($key)) { $events[$key] = @() }
    $events[$key] += $file.BaseName.Split('-')[-1]
}
$proofs = @()
foreach ($key in $identities.Keys | Sort-Object) {
    $d = $identities[$key]
    if ('spawn' -in $events[$key] -and 'exit' -notin $events[$key]) { throw "Unobserved retained child exit: $key" }
    $h = [DiagnosisProcessProof]::OpenProcess(0x00101000,$false,[uint32]$d.pid)
    $proof = [ordered]@{pid=$d.pid; creation_filetime=$d.creation_filetime}
    if ($h -eq [IntPtr]::Zero) {
        $err = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
        if ($err -eq 87) { $proof.status = 'EXITED_ABSENT' }
        elseif ($err -eq 5) {
            $rows = @(Get-CimInstance Win32_Process -Filter "ProcessId = $($d.pid)" -ErrorAction Stop)
            if ($rows.Count -ne 1) { throw "Access denial without positive identity reuse proof: $key" }
            $ticks = $rows[0].CreationDate.ToUniversalTime().ToFileTimeUtc()
            if ([decimal]::Floor([decimal]$ticks/10) -le [decimal]::Floor([decimal]$d.creation_filetime/10)) { throw "No later CreationDate after denial: $key" }
            $proof.status='EXITED_PID_REUSED_CIM_VERIFIED';$proof.current_creation_filetime=$ticks;$proof.initial_access_error=5
        } else { throw "OpenProcess error $err for $key" }
    } else {
        try {
            [long]$created=0;[long]$exited=0;[long]$kernelTime=0;[long]$userTime=0
            if (-not [DiagnosisProcessProof]::GetProcessTimes($h,[ref]$created,[ref]$exited,[ref]$kernelTime,[ref]$userTime)) { throw "GetProcessTimes failed: $key" }
            if ($created -gt [long]$d.creation_filetime) { $proof.status='EXITED_PID_REUSED';$proof.current_creation_filetime=$created }
            elseif ($created -ne [long]$d.creation_filetime) { throw "Unexpected earlier identity: $key" }
            elseif ([DiagnosisProcessProof]::WaitForSingleObject($h,0) -eq 0) { $proof.status='EXITED_HANDLE_SIGNALED' }
            else { throw "Identity still running or wait failed: $key" }
        } finally { [void][DiagnosisProcessProof]::CloseHandle($h) }
    }
    $proofs += $proof
}
$now = [DateTime]::UtcNow.ToString('o')
$closure = [ordered]@{
    schema='pr18-diagnosis-closure-v1'; result='PASS'; completed_utc=$now;
    decision='DIAGNOSIS_COMPLETE_NO_TRAINING'; histories=256; positions=2048; searches=7680;
    registered_identities_exited=$proofs.Count; registered_processes=$proofs;
    archive=$archive; compiler_descendants=$exitAudit.compiler_descendants;
    auditor_and_packager_exit='All their journalled exact identities are included in the strict exit proof.';
    control_plane='This PowerShell finalizer is synchronous; its own completion is observed by the calling exec session.';
    champion='PR18 unchanged'; training_started=$false; new_candidate=$false; automatic_successor=$false;
    released=$true; finalizer_sha256=(Get-FileHash -LiteralPath $PSCommandPath -Algorithm SHA256).Hash.ToLowerInvariant()
}
$lease = Read-Json $leasePath
if ($lease.experiment -ne 'pr18-diagnosis-v1' -or $lease.released) { throw 'Unexpected lease ownership' }
Write-Json (Join-Path $task 'lease-before-release.json') $lease
Write-Json (Join-Path $task 'closure.json') $closure
$lease.state='CLOSED_DIAGNOSIS';$lease.released=$true
$lease | Add-Member -NotePropertyName released_utc -NotePropertyValue $now -Force
$lease | Add-Member -NotePropertyName closure_evidence -NotePropertyValue '.local/pr18-diagnosis-v1/closure.json' -Force
$lease | Add-Member -NotePropertyName decision -NotePropertyValue 'No training; prioritize horizon/liquidation diagnosis; NNUE capacity unproven' -Force
Write-Json $leasePath $lease
$prefix = @"
pr18-diagnosis-v1 is CLOSED_DIAGNOSIS. Fixed bank completed and audited:
256 histories / 2048 states / 7680 queries. No training, candidate or strength claim.
All $($proofs.Count) registered process identities exited, including auditors and packager.
The $($archive.entries)-entry local evidence ZIP was hash-verified entry by entry;
adjacent closure.json binds its hash and proves exits after packaging. Lease released.
Read docs/petrified/pr18-diagnosis-results-2026-10-31.md. PR18 remains champion.
No automatic next phase; both GUIs stay closed, T8 paused, no OB or remote work.
Earlier diagnostic stages below are historical and must not restart any workload.

"@
foreach ($path in @((Join-Path $root 'AGENTS.md'),(Join-Path $root 'docs/petrified/STATUS.md'))) {
    [IO.File]::WriteAllText($path,$prefix + "`n" + [IO.File]::ReadAllText($path),[Text.UTF8Encoding]::new($false))
}
$summary = [ordered]@{}
foreach ($key in $closure.Keys) { if ($key -ne 'registered_processes') { $summary[$key]=$closure[$key] } }
$summary.closure_sha256=(Get-FileHash -LiteralPath (Join-Path $task 'closure.json') -Algorithm SHA256).Hash.ToLowerInvariant()
Write-Json (Join-Path $root 'docs/petrified/pr18-diagnosis-closure-2026-10-31.json') $summary
$summary | ConvertTo-Json -Depth 8 -Compress
