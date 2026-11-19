$p='src\server.hpp'; $t=[IO.File]::ReadAllText($p)
$t=$t.Replace('std::lock_guard<std::mutex> lk(jpeg_hw_mutex());','std::lock_guard<std::mutex> lk(jpeg_hw_mutex(1));')
[IO.File]::WriteAllText($p,$t)
