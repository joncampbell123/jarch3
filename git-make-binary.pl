#!/usr/bin/perl
# INTERNAL CASTUS USE ONLY

$project = 'isp-utils-v4';

$branch = `git branch | grep '^\*' | sed -e 's/^\* //'`; chomp $branch;
$branchfname = "-branch-$branch" if $branch ne "";
print "Current branch: $branch\n";

print "Ensuring the build tree is clean...\n";
$x = system("./git-update-all-wo-push");
die unless $x == 0;

if (!open(S,"git --no-pager log --max-count=1 |")) { exit 1; }
my $lcommit = "x";
my $i,$lcdate = "unknown";
foreach my $line (<S>) {
	chomp $line;

	if ($line =~ m/^commit +/) {
		$lcommit = lc($');
		$lcommit =~ s/ +$//;
	}
	else {
		my @nv = split(/: +/,$line);
		my $value = $nv[1];
		my $name = $nv[0];

		if ($name =~ m/^Date$/i) {
			# Fri Apr 15 08:51:32 2011 -0700
			#  0   1   2  3        4     5
			my @b = split(/ +/,$value);

			$mon = lc($b[1]);
			$day = $b[2];
			$time = $b[3];
			$year = $b[4];
			$mon = 1 if $mon eq "jan";
			$mon = 2 if $mon eq "feb";
			$mon = 3 if $mon eq "mar";
			$mon = 4 if $mon eq "apr";
			$mon = 5 if $mon eq "may";
			$mon = 6 if $mon eq "jun";
			$mon = 7 if $mon eq "jul";
			$mon = 8 if $mon eq "aug";
			$mon = 9 if $mon eq "sep";
			$mon = 10 if $mon eq "oct";
			$mon = 11 if $mon eq "nov";
			$mon = 12 if $mon eq "dec";
			$mon = $mon+0;
			$date = sprintf("%04u%02u%02u",$year+0,$mon,$day+0);
			$time =~ s/://g;
			$lcdate = $date."-".$time;
		}
	}
}
close(S);

my $pwd = `pwd`; chomp $pwd;
my $filename = "../".($as ne "" ? $as : $project)."-$lcdate-commit-$lcommit-binary$branchfname.tar";
if (!( -f "$filename.xz" )) {
	print "Building source\n";
	print "  to: $filename\n";

	$x = system("./build >/dev/null");
	die unless $x == 0;
	$x = system("rm -Rf bin-tmp && mkdir bin-tmp");
	die unless $x == 0;
	$x = system("make DESTDIR=$pwd/bin-tmp INSTALL_ROOT=$pwd/bin-tmp install");
	die unless $x == 0;

	$x = system("cd bin-tmp && tar -cvf ../$filename *"); # --exclude=.git
	die unless $x == 0;
	print "Packing to XZ\n";
	$x = system("xz -6e $filename");
	die unless $x == 0;
}

$x = system("./git-update-all-wo-push");
die unless $x == 0;

