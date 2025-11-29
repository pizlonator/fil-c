# Compiling libva/libva-utils deps

$ProgressPreference = "SilentlyContinue"
$MyPath = $MyInvocation.MyCommand.Path | Split-Path -Parent
. "$MyPath\mesa_init_msvc.ps1"

Remove-Item -Recurse -Force -ErrorAction SilentlyContinue "deps" | Out-Null
$depsInstallPath="C:\mesa-deps"

Write-Host "Cloning libva at:"
Get-Date
git clone https://github.com/intel/libva.git deps/libva
if (!$?) {
  Write-Host "Failed to clone libva repository"
  Exit 1
}

Write-Host "Cloning libva finished at:"
Get-Date

Write-Host "Building libva at:"
Get-Date

Push-Location -Path ".\deps\libva"
Write-Host "Checking out libva..."
git checkout 2.21.0
Pop-Location

# libva already has a build dir in their repo, use builddir instead
$libva_build = New-Item -ItemType Directory -Path ".\deps\libva" -Name "builddir"
Push-Location -Path $libva_build.FullName
meson .. -Dprefix="$depsInstallPath"
ninja -j32 install
$buildstatus = $?
Pop-Location
Remove-Item -Recurse -Force -ErrorAction SilentlyContinue -Path $libva_build
if (!$buildstatus) {
  Write-Host "Failed to compile libva"
  Exit 1
}

Write-Host "Building libva finished at:"
Get-Date

Write-Host "Cloning libva-utils at:"
Get-Date
git clone https://github.com/intel/libva-utils.git deps/libva-utils
if (!$?) {
  Write-Host "Failed to clone libva-utils repository"
  Exit 1
}

Write-Host "Cloning libva-utils finished at:"
Get-Date

Write-Host "Building libva-utils at:"
Get-Date

Push-Location -Path ".\deps\libva-utils"
Write-Host "Checking out libva-utils..."
git checkout 2.21.0
Pop-Location

Write-Host "Building libva-utils"
# libva-utils already has a build dir in their repo, use builddir instead
$libva_utils_build = New-Item -ItemType Directory -Path ".\deps\libva-utils" -Name "builddir"
Push-Location -Path $libva_utils_build.FullName
meson .. -Dprefix="$depsInstallPath" --pkg-config-path="$depsInstallPath\lib\pkgconfig;$depsInstallPath\share\pkgconfig"
ninja -j32 install
$buildstatus = $?
Pop-Location
Remove-Item -Recurse -Force -ErrorAction SilentlyContinue -Path $libva_utils_build
if (!$buildstatus) {
  Write-Host "Failed to compile libva-utils"
  Exit 1
}

Write-Host "Building libva-utils finished at:"
Get-Date
