$p='src\task.hpp'; $t=[IO.File]::ReadAllText($p)
$t=$t.Replace('if (start_gateway(url)) {','if (start_gateway(url, reuse_ports)) {')
$t=$t.Replace('if (start_gateway(url2)) {','if (start_gateway(url2, reuse_ports)) {')
[IO.File]::WriteAllText($p,$t)
