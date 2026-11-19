$p='src\task.hpp'; $t=[IO.File]::ReadAllText($p)
$t=$t.Replace('`r`n', "`r`n")
[IO.File]::WriteAllText($p,$t)
