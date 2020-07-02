UC.Operator.exe Module Attach TestExe.exe UC.Agent.dll UC.Tinker.dll TestDll.dll
UC.Operator.exe RVA < ..\..\TestExe-Patch.txt > ..\..\TestExe-Patch.rva
UC.Operator.exe Order TestExe.exe UC.Tinker.dll UCTinker_Replace ..\..\TestExe-Patch.rva
