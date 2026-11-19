$p='docs\PROGRESS.md'; $t=[IO.File]::ReadAllText($p)
$t=$t.Replace("iteration: 105`r`nupdated: 2026-11-19","iteration: 106`r`nupdated: 2026-11-19")
[IO.File]::WriteAllText($p,$t)
