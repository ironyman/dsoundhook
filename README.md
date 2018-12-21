# Clone
```
git clone --recurse-submodules git@github.com:ironyman/dsoundhook.git
```
# Build detours

open "developer command prompt for vs 2017"
```
%comspec% /k "C:\Program Files (x86)\Microsoft Visual Studio\2017\Enterprise\Common7\Tools\VsDevCmd.bat"
```

go to detours directory
```
cd src
nmake
cd ..\samples
nmake
```
it will produce header and x86 lib in root of detours directory.

# Build this solution
Build with visual studio.
# Run
```
mkdir c:\dump\
.\Detours\bin.X86\withdll.exe /d:debug\dsoundhook.dll "C:\Users\$env:UserName\AppData\Roaming\Spotify\Spotify.exe"
```
