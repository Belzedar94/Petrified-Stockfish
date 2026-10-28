param(
    [string]$Engine,
    [string]$Network,
    [switch]$InstallDependencies
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$guiRoot = Join-Path $projectRoot '.local/fairyground'
$integrationRoot = Join-Path $projectRoot 'integrations/fairyground'
$revision = '5f7392c3416b3949a3352d729f4e32368f03bade'
if (!$Engine) { $Engine = Join-Path $projectRoot 'src/petrified-bmi2-gui.exe' }
if (!$Network) { $Network = Join-Path $projectRoot '../../../Match script/petrified_run4test_e19_l085.nnue' }
$Engine = (Resolve-Path -LiteralPath $Engine).Path
$Network = (Resolve-Path -LiteralPath $Network).Path
if ((Get-FileHash -LiteralPath $Network -Algorithm SHA256).Hash.ToLowerInvariant() -ne '345d67b733991ae5beb2d3a6ea40d54ea01abb1fb699aa39e6f6406495f4d933') {
    throw 'This Fairyground profile requires the frozen run4test network.'
}
if (@(Get-NetTCPConnection -State Listen -LocalPort 5185,5186 -ErrorAction SilentlyContinue).Count) {
    throw 'Stop this Fairyground instance with scripts/stop_fairyground.ps1 before reconfiguring it.'
}
if (!(Test-Path -LiteralPath $guiRoot)) {
    git clone --no-checkout https://github.com/ianfab/fairyground.git $guiRoot
    if ($LASTEXITCODE) { throw 'Fairyground clone failed.' }
    git -C $guiRoot checkout --detach $revision
    if ($LASTEXITCODE) { throw 'Fairyground checkout failed.' }
}
if ((git -C $guiRoot rev-parse HEAD).Trim() -ne $revision) { throw 'Unexpected Fairyground revision; existing checkout preserved.' }
if ((Get-FileHash -LiteralPath (Join-Path $guiRoot 'package-lock.json') -Algorithm SHA256).Hash.ToLowerInvariant() -ne '24545295e38f03dd19541d7faf73d31c45714a8f549a9d9676e7d317ddf51beb') {
    throw 'Unexpected Fairyground dependency lockfile.'
}
$patch = Join-Path $integrationRoot 'native-petrified.patch'
git -C $guiRoot apply --reverse --check $patch 2>$null
if ($LASTEXITCODE) {
    git -C $guiRoot apply --check $patch
    if ($LASTEXITCODE) { throw 'Local Fairyground edits conflict with this profile; checkout preserved.' }
    git -C $guiRoot apply $patch
    if ($LASTEXITCODE) { throw 'Could not apply the native Petrified profile.' }
}
Copy-Item -LiteralPath (Join-Path $integrationRoot 'PetrifiedLocalFeature.js') -Destination (Join-Path $guiRoot 'src/js/PetrifiedLocalFeature.js')
Push-Location $guiRoot
try {
    if ($InstallDependencies -or !(Test-Path -LiteralPath 'node_modules/.bin/browserify.cmd')) {
        npm ci --ignore-scripts
        if ($LASTEXITCODE) { throw 'Fairyground dependency installation failed.' }
    }
    New-Item -ItemType Directory -Path 'public/lib/canvas' -Force | Out-Null
    Copy-Item -Path 'node_modules/fairy-stockfish-nnue.wasm/*' -Destination 'public/lib'
    Copy-Item -LiteralPath 'node_modules/ffish-es6/ffish.wasm' -Destination 'public/ffish.wasm'
    Copy-Item -Path 'node_modules/gif.js/dist/*.js' -Destination 'public'
    & './node_modules/.bin/browserify.cmd' -p esmify src/js/main.js -o public/bundle.js
    if ($LASTEXITCODE) { throw 'Fairyground board bundle failed.' }
    & './node_modules/.bin/browserify.cmd' -p esmify src/js/variantEditor.js -o public/lib/varianteditor.js
    if ($LASTEXITCODE) { throw 'Fairyground editor bundle failed.' }
    Copy-Item -LiteralPath 'node_modules/mithril/mithril.min.js' -Destination 'public/lib/mithril.min.js'
    Copy-Item -Path 'src/js/*Feature.js' -Destination 'public/lib'
    Copy-Item -Path 'src/js/*.canvas.worker.js' -Destination 'public/lib/canvas'
    Copy-Item -LiteralPath 'src/js/sw.js' -Destination 'public/sw.js'
    Copy-Item -LiteralPath 'src/js/server.js' -Destination 'server.js'
    Copy-Item -Path 'src/html/*' -Destination 'public'
    $engineCwd = Split-Path -Parent $Engine
    $profile = "Petrified-Stockfish + run4test;$Engine;$engineCwd;UCI;UCI_Variant=petrified,EvalFile=$Network,Threads=1,Hash=128,Ponder=false`n"
    [IO.File]::WriteAllText((Join-Path $guiRoot 'EngineList.txt'), $profile, [Text.UTF8Encoding]::new($false))
    $manifest = [ordered]@{
        schema = 'petrified-fairyground-v1'; fairyground_commit = $revision
        engine = $Engine; engine_sha256 = (Get-FileHash -LiteralPath $Engine -Algorithm SHA256).Hash.ToLowerInvariant()
        network = $Network; network_sha256 = (Get-FileHash -LiteralPath $Network -Algorithm SHA256).Hash.ToLowerInvariant()
        patch_sha256 = (Get-FileHash -LiteralPath $patch -Algorithm SHA256).Hash.ToLowerInvariant()
        threads = 1; hash_mb = 128; url = 'http://127.0.0.1:5185/public/index.html?variant=petrified'
    }
    $manifest | ConvertTo-Json | Set-Content -LiteralPath 'petrified-profile.json' -Encoding UTF8
} finally { Pop-Location }
Write-Output 'Configured. Start with scripts/start_fairyground.ps1 -OpenBrowser.'
