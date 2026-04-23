@ECHO OFF

SET SCRIPT_DIR=%~dp0

::创建快捷方式
echo Set oWS = WScript.CreateObject("WScript.Shell") > temp.vbs
echo sLinkFile = "%USERPROFILE%\Desktop\瞳乐辅注工具.lnk" >> temp.vbs
echo Set oLink = oWS.CreateShortcut(sLinkFile) >> temp.vbs
echo oLink.TargetPath = "%SCRIPT_DIR%\tl_assistant.exe" >> temp.vbs
echo oLink.Arguments = "--console=false" >> temp.vbs
echo oLink.WorkingDirectory = "%SCRIPT_DIR%" >> temp.vbs
echo oLink.Description = "瞳乐辅注工具"  >> temp.vbs
echo oLink.WindowStyle = 3  >> temp.vbs
echo oLink.Save >> temp.vbs
cscript /nologo temp.vbs
del temp.vbs

pause