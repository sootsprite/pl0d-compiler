#!/usr/bin/perl
#
# PL/0'仮想機械用アセンブリ言語処理系 pl0das.pl
#
# 作成者: 鈴木　徹也
#
# 2007/03/01 初版
# 2008/05/22 ６つの関数execOprEq, execOprLs, execOprGr, execOprNeq, execOprLseq, execOprGrEqについて、
#            実行結果が偽の値の場合、数値0ではなく、ヌル文字がスタックに積まれるバグを修正
# 2009/05/29 pcの範囲チェックについて、範囲の修正とエラーメッセージ出力の追加。
#            一部サブルーチン名を変更。
use strict;

# コマンドライン引数のチェック
if (@ARGV == 0) {
	print "usage: pl0das.pl foo.s\n";
	exit 1;
}

#my $MAXCODE = 200; # 目的コードの最大長
my $MAXMEM = 2000; # 実行時スタックの最大長
my $MAXREG = 20;   # 演算レジスタスタックの最大長
my $MAXLEVEL =  5; # ブロックの最大深さ

# ソースファイルの各行を解析するサブルーチンリスト
# 各要素はサブルーチンへのポインタ（順序に意味あり）
my @readProcs = (
	\&readLit, \&readOprNeg, \&readOprAdd, \&readOprSub, \&readOprMul,
	\&readOprDiv, \&readOprOdd, \&readOprEq, \&readOprLs, \&readOprGr,
	\&readOprNeq, \&readOprLseq, \&readOprGreq, \&readOprWrt, \&readOprWrl,
	\&readLod, \&readSto, \&readCal, \&readRet, \&readIct, \&readJmp,
	\&readJpc, \&readEmptyLine,
	\&declareErrror # 必ず最後に置く
);

# ソースファイルの行番号
my $lineNumber = 1;

# エラーカウンタ
my $errorCounter = 0;

# コードの列
my $nextAddress = 0; # 次に記録する位置
my @code = ();       # コードが記録される配列

sub putCode {
	my ($code) = @_;
	push(@code, $code);
	$nextAddress++;
}

# ディスプレイ（スタックトップの待避場所）
my @display = ();
for(my $i = 0; $i < $MAXLEVEL; $i++){
	push(@display, 0);
}

sub printDisplay {
	print "display: ", join(',', @display), "\n";
}

# 実行時のスタック
my @stack = ();
my $top = 0;
for(my $i = 0; $i < $MAXREG; $i++){
	push(@stack, 0);
}


sub printStack {
	print "top    : ", $top, "\n";
	my @x = @stack;
	$x[$top] = $x[$top] . "<-top";
#    for(my $i = 0; $i < @display; $i++){
#	$x[$display[$i]] = $x[$display[$i]]. '<-d[' . $i. ']';
#    }
	print "stack  : ", join(',', @x), "\n";
}

# プログラムカウンタ
my $pc = 0;

# 出力の内容
my @output = ();

sub printOutput {
	print @output;
}

sub writeToOutput {
	push(@output, @_);
}

# 仮想機械の状態の表示
sub printStatus {
	print "状態\n";
	print "pc     : ", $pc, "\n";
	&printStack;
	&printDisplay;
	print "output :\n";
	&printOutput;
	print "\n";
}

#
# ラベル表（ハッシュ）
#
# キー: ラベル名
# 値: ハッシュ キー:value, 値: 参照しているコード
#
my %labels = ();

# ラベルをその値とともに登録する。
sub setLabelWithValue {
	my ($labelName, $value) = @_;
	if (!defined($labels{$labelName})) {
		# はじめて登録する
		$labels{$labelName} = {'value' => $value};
	} elsif (!defined($labels{$labelName}->{'value'})){
		# すでに参照はされているが値は未定義
		$labels{$labelName}->{'value'} = $value;
		# バックパッチ
		foreach my $code (@{$labels{$labelName}->{'refBy'}}){
			$code->{'address'} = $value;
		}
	} else {
		# エラー: 再定義
		printf "%3d:The label '%s' is redefined.\n", $lineNumber, $labelName;
		$errorCounter++;
	}
}

# ラベルを値なしで登録する。
sub setLabelWithoutValue {
	my ($labelName) = @_;
	$labels{$labelName} = {'refBy' => []};
}

# ラベルの値を得る。
sub getLabelValue {
	my ($labelName) = @_;
	if (!defined($labels{$labelName})){
		setLabelWithoutValue($labelName);
		return undef; //
	} elsif (!defined($labels{$labelName}->{'value'})){
		return undef;
	} else {
		return $labels{$labelName}->{'value'};
	}
}

# バックパッチのためにコードを登録する。
sub setCodeForBackpatch {
	my ($labelName, $code) = @_;
	push(@{$labels{$labelName}->{'refBy'}}, $code);
}

# 未定義ラベルがあるかを調べる。
sub checkUndefinedLabel {
	foreach my $key (keys(%labels)){
		my $value = $labels{$key}->{'value'};
		if (!defined($value)){
			printf "The label '%s' is not defined.\n", $key;
			$errorCounter++;
		}
	}
	return 0;
}

#
# 解析
#
open(SRC, $ARGV[0]) || die "Can't open $ARGV[0]\n";
while(my $line = <SRC>){
	chomp $line;

	if ($line =~ /^\s*([_a-zA-Z][a-zA-Z0-9]+):(.*)$/){
		# ラベルの登録
		setLabelWithValue($1, $nextAddress);
		$line = $2;
	}
	if ($line =~ /^([^;]*)(;.*)$/){
		# コメント除去
		$line = $1;
	}
	# 残り部分の解析
	foreach my $proc (@readProcs) {
		last if &$proc($line);
	}
	$lineNumber++;
}
close(SRC);

# エラーチェック
&checkUndefinedLabel;

if ($errorCounter > 0) {
	exit 1;
}


#
# 実行
#
print "-------------------\n";
do {
#    print "pc:", $pc, "\n";
	if ($pc >= @code || $pc < 0) {
		printf "pc(=%d)が範囲外です。\n", $pc;
		print  "異常終了\n";
		exit 1;
	}

	my $code = $code[$pc];
	my $opcode = $code->{'opcode'};
	my $line = $code->{'line'};

	&printStatus;
	print "-------------------\n";
	print "実行する命令\n";
	printf "%s\n", $line;
	print "-------------------\n";

	$pc++;
	&$opcode($code);
} while ($pc != 0);

&printStatus;
print "-------------------\n";
print "終了\n";

exit 0;

#
# ラベル定義(未使用)
#
sub readEQU {
	my ($line) = @_;
	print "readEQU\n";
	if ($line =~ /^\s*([a-zA-Z][a-zA-Z0-9_]*)\s+(EQU|equ)\s+([^\s]+)\s*$/){
		print "match\n";
		return 1; # done
	}
	return 0;
}

#
# 空行の解析
#
sub readEmptyLine {
	my ($line) = @_;
	if ($line =~ /^\s*$/) {
		return 1; # done
	}
	return 0;
}

#
# 無条件にエラー出力
#
sub declareErrror {
	printf "%3d:Syntax error\n", $lineNumber;
	$errorCounter++;
	return 1;
}

#
# 各命令の解析
#
sub readLit {
	my ($line) = @_;
	if ($line =~ /^\s*lit\s*,\s*(-?\d+)\s*$/){
		my $code = {'opcode' => \&execLit, 'value' => $1, 'line' => $line};
		putCode($code);
		return 1; # done
	}
	return 0;
}

sub readOprNeg {
	my ($line) = @_;
	if ($line =~ /^\s*opr\s*,\s*neg\s*$/){
		my $code = {'opcode' => \&execOprNeg, 'line' => $line};
		putCode($code);
		return 1; # done
	}
	return 0;
}

sub readOprAdd {
	my ($line) = @_;
	if ($line =~ /^\s*opr\s*,\s*add\s*$/){
		my $code = {'opcode' => \&execOprAdd, 'line' => $line};
		putCode($code);
		return 1; # done
	}
	return 0;
}

sub readOprSub {
	my ($line) = @_;
	if ($line =~ /^\s*opr\s*,\s*sub\s*$/){
		my $code = {'opcode' => \&execOprSub, 'line' => $line};
		putCode($code);
		return 1; # done
	}
	return 0;
}

sub readOprMul {
	my ($line) = @_;
	if ($line =~ /^\s*opr\s*,\s*mul\s*$/){
		my $code = {'opcode' => \&execOprMul, 'line' => $line};
		putCode($code);
		return 1; # done
	}
	return 0;
}

sub readOprDiv {
	my ($line) = @_;
	if ($line =~ /^\s*opr\s*,\s*div\s*$/){
		my $code = {'opcode' => \&execOprDiv, 'line' => $line};
		putCode($code);
		return 1; # done
	}
	return 0;
}

sub readOprOdd {
	my ($line) = @_;
	if ($line =~ /^\s*opr\s*,\s*odd\s*$/){
		my $code = {'opcode' => \&execOprOdd, 'line' => $line};
		putCode($code);
		return 1; # done
	}
	return 0;
}

sub readOprEq {
	my ($line) = @_;
	if ($line =~ /^\s*opr\s*,\s*eq\s*$/){
		my $code = {'opcode' => \&execOprEq, 'line' => $line};
		putCode($code);
		return 1; # done
	}
	return 0;
}

sub readOprLs {
	my ($line) = @_;
	if ($line =~ /^\s*opr\s*,\s*ls\s*$/){
		my $code = {'opcode' => \&execOprLs, 'line' => $line};
		putCode($code);
		return 1; # done
	}
	return 0;
}

sub readOprGr {
	my ($line) = @_;
	if ($line =~ /^\s*opr\s*,\s*gr\s*$/){
		my $code = {'opcode' => \&execOprGr, 'line' => $line};
		putCode($code);
		return 1; # done
	}
	return 0;
}

sub readOprNeq {
	my ($line) = @_;
	if ($line =~ /^\s*opr\s*,\s*neq\s*$/){
		my $code = {'opcode' => \&execOprNeq, 'line' => $line};
		putCode($code);
		return 1; # done
	}
	return 0;
}

sub readOprLseq {
	my ($line) = @_;
	if ($line =~ /^\s*opr\s*,\s*lseq\s*$/){
		my $code = {'opcode' => \&execOprLseq, 'line' => $line};
		putCode($code);
		return 1; # done
	}
	return 0;
}

sub readOprGreq {
	my ($line) = @_;
	if ($line =~ /^\s*opr\s*,\s*greq\s*$/){
		my $code = {'opcode' => \&execOprGreq, 'line' => $line};
		putCode($code);
		return 1; # done
	}
	return 0;
}

sub readOprWrt {
	my ($line) = @_;
	if ($line =~ /^\s*opr\s*,\s*wrt\s*$/){
		my $code = {'opcode' => \&execOprWrt, 'line' => $line};
		putCode($code);
		return 1; # done
	}
	return 0;
}

sub readOprWrl {
	my ($line) = @_;
	if ($line =~ /^\s*opr\s*,\s*wrl\s*$/){
		my $code = {'opcode' => \&execOprWrl, 'line' => $line};
		putCode($code);
		return 1; # done
	}
	return 0;
}

sub readLod {
	my ($line) = @_;
	if ($line =~ /^\s*lod\s*,\s*(\d+)\s*,\s*(-?\d+)\s*$/){
		my $code = {'opcode' => \&execLod, 'level' => $1, 'address' => $2, 'line' => $line};
		putCode($code);
		return 1; # done
	} elsif ($line =~ /^\s*lod\s*,\s*(\d+)\s*,\s*([_a-zA-Z][a-zA-Z0-9]+)\s*$/){
		my $labelName = $2;
		my $address = getLabelValue($labelName);
		my $code = {'opcode' => \&execLod, 'level' => $1, 'address' => $address, 'line' => $line};
		putCode($code);
		if (!defined($address)){
			setCodeForBackpatch($labelName, $code);
		}
		return 1; # done
	}
	return 0;
}

sub readSto {
	my ($line) = @_;
	if ($line =~ /^\s*sto\s*,\s*(\d+)\s*,\s*(-?\d+)\s*$/){
		my $code = {'opcode' => \&execSto, 'level' => $1, 'address' => $2, 'line' => $line};
		putCode($code);
		return 1; # done
	} elsif ($line =~ /^\s*sto\s*,\s*(\d+)\s*,\s*([_a-zA-Z][a-zA-Z0-9]+)\s*$/){
		my $labelName = $2;
		my $address = getLabelValue($labelName);
		my $code = {'opcode' => \&execSto, 'level' => $1, 'address' => $address, 'line' => $line};
		putCode($code);
		if (!defined($address)){
			setCodeForBackpatch($labelName, $code);
		}
		return 1; # done
	}
	return 0;
}

sub readCal {
	my ($line) = @_;
	if ($line =~ /^\s*cal\s*,\s*(\d+)\s*,\s*(-?\d+)\s*$/){
		my $code = {'opcode' => \&execCal, 'level' => $1, 'address' => $2, 'line' => $line};
		putCode($code);
		return 1; # done
	} elsif ($line =~ /^\s*cal\s*,\s*(\d+)\s*,\s*([_a-zA-Z][a-zA-Z0-9]+)\s*$/){
		my $labelName = $2;
		my $address = getLabelValue($labelName);
		my $code = {'opcode' => \&execCal, 'level' => $1, 'address' => $address, 'line' => $line};
		putCode($code);
		if (!defined($address)){
			setCodeForBackpatch($labelName, $code);
		}
		return 1; # done
	}
	return 0;
}

sub readRet {
	my ($line) = @_;
	if ($line =~ /^\s*ret\s*,\s*(\d+)\s*,\s*(-?\d+)\s*$/){
		my $code = {'opcode' => \&execRet, 'level' => $1, 'address' => $2, 'line' => $line};
		putCode($code);
		return 1; # done
	} elsif ($line =~ /^\s*ret\s*,\s*(\d+)\s*,\s*([_a-zA-Z][a-zA-Z0-9]+)\s*$/){
		my $labelName = $2;
		my $address = getLabelValue($labelName);
		my $code = {'opcode' => \&execRet, 'level' => $1, 'address' => $address, 'line' => $line};
		putCode($code);
		if (!defined($address)){
			setCodeForBackpatch($labelName, $code);
		}
		return 1; # done
	}
	return 0;
}

sub readIct {
	my ($line) = @_;
	if ($line =~ /^\s*ict\s*,\s*(-?\d+)\s*$/){
		my $code = {'opcode' => \&execIct, 'value' => $1, 'line' => $line};
		putCode($code);
		return 1; # done
	}
	return 0;
}

sub readJmp {
	my ($line) = @_;
	if ($line =~ /^\s*jmp\s*,\s*(\d+)\s*$/){
		my $code = {'opcode' => \&execJmp, 'address' => $1, 'line' => $line};
		putCode($code);
		return 1; # done
	} elsif ($line =~ /^\s*jmp\s*,\s*([_a-zA-Z][a-zA-Z0-9]+)\s*$/){
		my $labelName = $1;
		my $address = getLabelValue($labelName);
		my $code = {'opcode' => \&execJmp, 'address' => $address, 'line' => $line};
		putCode($code);
		if (!defined($address)){
			setCodeForBackpatch($labelName, $code);
		}
		return 1; # done
	}
	return 0;
}

sub readJpc {
	my ($line) = @_;
	if ($line =~ /^\s*jpc\s*,\s*(\d+)\s*$/){
		my $code = {'opcode' => \&execJpc, 'address' => $1, 'line' => $line};
		putCode($code);
		return 1; # done
	} elsif ($line =~ /^\s*jpc\s*,\s*([_a-zA-Z][a-zA-Z0-9]+)\s*$/){
		my $labelName = $1;
		my $address = getLabelValue($labelName);
		my $code = {'opcode' => \&execJpc, 'address' => $address, 'line' => $line};
		putCode($code);
		if (!defined($address)){
			setCodeForBackpatch($labelName, $code);
		}
		return 1; # done
	}
	return 0;
}


#
# 各命令の実行
#
sub execLit {
	my ($code) = @_;
	my $opr = $code->{'value'};
	$stack[$top++] = $opr;
}

sub execOprNeg {
	$stack[$top-1] = - $stack[$top-1];
}

sub execOprAdd {
	--$top;
	$stack[$top-1] += $stack[$top];
}

sub execOprSub {
	--$top;
	$stack[$top-1] -= $stack[$top];
}

sub execOprMul {
	--$top;
	$stack[$top-1] *= $stack[$top];
}

sub execOprDiv {
	--$top;
	$stack[$top-1] =  int($stack[$top-1] / $stack[$top]);
}

sub execOprOdd {
	$stack[$top-1] = $stack[$top-1] & 1;
}

sub execOprEq {
	--$top;
	$stack[$top-1] = ($stack[$top-1] == $stack[$top]) + 0;
}

sub execOprLs {
	--$top;
	$stack[$top-1] = ($stack[$top-1] < $stack[$top]) + 0;
}

sub execOprGr {
	--$top;
	$stack[$top-1] = ($stack[$top-1] > $stack[$top]) + 0;
}

sub execOprNeq {
	--$top;
	$stack[$top-1] = ($stack[$top-1] != $stack[$top]) + 0;
}

sub execOprLseq {
	--$top;
	$stack[$top-1] = ($stack[$top-1] <= $stack[$top]) + 0;
}

sub execOprGreq {
	--$top;
	$stack[$top-1] = ($stack[$top-1] >= $stack[$top]) + 0;
}

sub execOprWrt {
	&writeToOutput($stack[--$top].' ');
}

sub execOprWrl {
	&writeToOutput("\n");
}

sub execLod {
	my ($code) = @_;
	my $level = $code->{'level'};
	my $address = $code->{'address'};
	$stack[$top++] = $stack[$display[$level] + $address];
}

sub execSto {
	my ($code) = @_;
	my $level = $code->{'level'};
	my $address = $code->{'address'};
	$stack[$display[$level] + $address] = $stack[--$top];
}

sub execCal {
	my ($code) = @_;
	my $level = $code->{'level'};
	my $address = $code->{'address'};

	my $lev = $level +1;
	# levelは呼び出しもとのレベル
	# 呼び出し先のレベルlevはそれに＋１したもの　
	$stack[$top] = $display[$lev]; # display[lev]の退避
	$stack[$top+1] = $pc; # 戻り番地の記録
	$display[$lev] = $top;
	# 現在のtopがcalleeのブロックの先頭番地
	$pc = $address;
}

sub execRet {
	my ($code) = @_;
	my $level = $code->{'level'};
	my $address = $code->{'address'};

	my $temp = $stack[--$top]; # スタックのトップにあるものが返す値
	$top = $display[$level];   # topを呼ばれたときの値に戻す
	$display[$level] = $stack[$top]; # 壊したディスプレイの回復
	$pc = $stack[$top+1];
	$top -= $address; # 実引数の分だけトップを戻す
	$stack[$top++] = $temp; # 返す値をスタックのトップへ
}

sub execIct {
	my ($code) = @_;
	my $value = $code->{'value'};
	$top += $value;
	if ($top >= $MAXMEM - $MAXREG) {
		die "stack overflow";
	}
}

sub execJmp {
	my ($code) = @_;
	my $nextPc = $code->{'address'};
	$pc = $nextPc;
}

sub execJpc {
	my ($code) = @_;
	my $nextPc = $code->{'address'};
	if ($stack[--$top] == 0){
		$pc = $nextPc;
	}
}
