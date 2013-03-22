cd ../../../ext/speakertrack || exit -1
echo "run: Entering directory \`../../../ext/speakertrack'"
make install || exit -1
echo "run: Leaving directory \`../../../ext/speakertrack'"
cd -
make
./speakertrack_test $@
