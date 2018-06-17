#!/usr/bin/perl
#
# Ack!  gcc doesn't support unnamed unions -- we have to fake them.
#
# Expected format:
#
#struct blah
#{
#	union
#	{
#		type1 blah1;
#		type2 blah2;
#	};
#}

$lineno = 0;
$union = 0;
$ucount = 1;
while ( ($line = <STDIN>) ) {
	++$lineno;

	$line =~ s/\r//;
	if ( $line =~ /\sunion\s/ ) {
		if ( $union ) {
			die("Nested union at line $lineno -- Exiting.\n");
		}
		$union = 1;
	}
	if ( $union ) {
		if ( $line =~ /}/ ) {
			if ( $line =~ /};/ ) {
				chomp($line);
				$gnuline = $line;
				$gnuline =~ s/};/} u$ucount;/;
				$line = 
"#if defined(NONAMELESSUNION)\n$gnuline\n#else\n$line\n#endif\n";
				$ucount++;
			}
			$union = 0;
		}
	} else {
		if ( $line =~ /^}/ ) {
			$ucount = 1;
		}
	}
	print $line;
}
