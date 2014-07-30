#include "alsawrapper.c"

int main(int argc, char const *argv[])
{
    alsawrapper(CMD_RECORD, TYPE_WAVE, FMT_DAT, VU_STEREO, 2, 48000, 0, false, "test_rec");
    start();
    getc(stdin);
    stop();
    return 0;
}