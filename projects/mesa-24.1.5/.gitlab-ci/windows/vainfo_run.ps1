
function Deploy-Dependencies {
  param (
    [string] $deploy_directory
  )
  
  Write-Host "Copying libva runtime and driver at:"
  Get-Date

  # Copy the VA runtime binaries from the mesa built dependencies so the versions match with the built mesa VA driver binary
  $depsInstallPath="C:\mesa-deps"
  Copy-Item "$depsInstallPath\bin\vainfo.exe" -Destination "$deploy_directory\vainfo.exe"
  Copy-Item "$depsInstallPath\bin\va_win32.dll" -Destination "$deploy_directory\va_win32.dll"
  Copy-Item "$depsInstallPath\bin\va.dll" -Destination "$deploy_directory\va.dll"

  # Copy Agility SDK into D3D12 subfolder of vainfo
  New-Item -ItemType Directory -Force -Path "$deploy_directory\D3D12" | Out-Null
  Copy-Item "$depsInstallPath\bin\D3D12\D3D12Core.dll" -Destination "$deploy_directory\D3D12\D3D12Core.dll"
  Copy-Item "$depsInstallPath\bin\D3D12\d3d12SDKLayers.dll" -Destination "$deploy_directory\D3D12\d3d12SDKLayers.dll"

  # Copy WARP next to vainfo
  Copy-Item "$depsInstallPath\bin\d3d10warp.dll" -Destination "$deploy_directory\d3d10warp.dll"

  Write-Host "Copying libva runtime and driver finished at:"
  Get-Date
}

function Check-VAInfo-Entrypoint {
  param (
    [string] $vainfo_app_path,
    [string] $entrypoint
  )

  $vainfo_run_cmd = "$vainfo_app_path --display win32 --device 0 2>&1 | Select-String $entrypoint -Quiet"
  Write-Host "Running: $vainfo_run_cmd"
  $vainfo_ret_code= Invoke-Expression $vainfo_run_cmd
  if (-not($vainfo_ret_code)) {
      return 0
  }
  return 1
}

# Set testing environment variables
$successful_run=1
$testing_dir="$PWD\_install\bin" # vaon12_drv_video.dll is placed on this directory by the build
$vainfo_app_path = "$testing_dir\vainfo.exe"

# Deploy vainfo and dependencies
Deploy-Dependencies -deploy_directory $testing_dir

# Set VA runtime environment variables
$env:LIBVA_DRIVER_NAME="vaon12"
$env:LIBVA_DRIVERS_PATH="$testing_dir"

Write-Host "LIBVA_DRIVER_NAME: $env:LIBVA_DRIVER_NAME"
Write-Host "LIBVA_DRIVERS_PATH: $env:LIBVA_DRIVERS_PATH"

# Check video processing entrypoint is supported
# Inbox WARP/D3D12 supports this entrypoint with VA frontend shaders support (e.g no video APIs support required)
$entrypoint = "VAEntrypointVideoProc"

# First run without app verifier
Write-Host "Disabling appverifier for $vainfo_app_path and checking for the presence of $entrypoint supported..."
appverif.exe /disable * -for "$vainfo_app_path"
$result_without_appverifier = Check-VAInfo-Entrypoint -vainfo_app_path $vainfo_app_path -entrypoint $entrypoint
if ($result_without_appverifier -eq 1) {
  Write-Host "Process exited successfully."
} else {
  $successful_run=0
  Write-Error "Process exit not successful for $vainfo_run_cmd. Please see vainfo verbose output below for diagnostics..."
  # verbose run to print more info on error (helpful to investigate issues from the CI output)
  Invoke-Expression "$vainfo_app_path -a --display win32 --device help"
  Invoke-Expression "$vainfo_app_path -a --display win32 --device 0"
}

# Enable appverif and run again
Write-Host "Enabling appverifier for $vainfo_app_path and checking for the presence of $entrypoint supported..."
appverif.exe /logtofile enable
appverif.exe /verify "$vainfo_app_path"
appverif.exe /enable "Leak" -for "$vainfo_app_path"
$verifier_log_path="$testing_dir\vainfo_appverif_log.xml"
$result_with_appverifier = Check-VAInfo-Entrypoint -vainfo_app_path $vainfo_app_path -entrypoint $entrypoint
if ($result_with_appverifier -eq 1) {
  Write-Host "Process exited successfully."
  appverif.exe /logtofile disable
} else {
  Write-Host "Process failed. Please see Application Verifier log contents below."
  # Need to wait for appverif to exit before gathering log
  Start-Process -Wait -FilePath "appverif.exe" -ArgumentList "-export", "log", "-for", "$vainfo_app_path", "-with", "to=$verifier_log_path"
  Get-Content $verifier_log_path
  Write-Error "Process exit not successful for $vainfo_run_cmd."
  appverif.exe /logtofile disable
  $successful_run=0
}

if ($successful_run -ne 1) {
  Exit 1
}

