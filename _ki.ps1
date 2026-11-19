$p='docs\KNOWN-ISSUES.md'; $t=[IO.File]::ReadAllText($p)
$t=$t.Replace("iteration: 104`r`nupdated: 2026-11-19","iteration: 105`r`nupdated: 2026-11-19")
[IO.File]::WriteAllText($p,$t)
