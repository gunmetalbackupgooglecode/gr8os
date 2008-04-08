Building DDK headers..

<?php

system("echo y | del ddk\\*.* > nul");
//system("if not exists ddk md ddk");

$files = array(
	"cc.h",
	"common.h",
	"ex.h",
	"hal.h",
	"io.h",
	"kd.h",
	"init.h",
	"ke.h",
	"mm.h",
	"ob.h",
	"ps.h",
	"rtl.h"
	);
	
$start = "// begin_ddk";
$end = "// end_ddk";
	
foreach($files as $file)
{
	$f = file($file);
	$out = fopen("ddk/".$file, "w") or die("cant open ddk/".$file);
	
	fputs($out, 
		"//\r\n".
		"// <$file> built by header file parser at ".date("H:i:s  d M Y")."\r\n".
		"// This is a part of gr8os include files for GR8OS Driver & Extender Development Kit (DEDK)\r\n".
		"//\r\n".
		"\r\n"
		);
	
	for($i=0;$i<count($f);$i++)
	{
		$line = $f[$i];
		
		if (stristr($line, $start) !== false)
		{
			for ($i++; $i<count($f);$i++)
			{
				$line = $f[$i];
				
				if (stristr($line, $end) !== false)
					break;
				
				fputs($out, $line);
			}
		}
	}
	
	fclose($out);
}
?>
Completed

	