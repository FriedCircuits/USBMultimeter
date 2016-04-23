#include "cmd.h"
#include "shell.h"
#include "usbcfg.h"
#include "chprintf.h"

#define SHELL_WA_SIZE   THD_WORKING_AREA_SIZE(2048)

static thread_t* _chShellThread = 0;

static const ShellCommand _commands[] = {
	{NULL, NULL}
};

static const ShellConfig _shell_cfg1 = {
	(BaseSequentialStream*)&SDU1,
	_commands
};

bool cmdCreate(void)
{
	// Don't launch multiple times
	if (_chShellThread) {
		return true;
	}

	// Make sure that the interface is up and running
	if (SDU1.config->usbp->state != USB_ACTIVE) {
		return false;
	}

	// Initialize the chibios shell
	shellInit();

	// Create the actual shell thread.
	_chShellThread = shellCreate(&_shell_cfg1, SHELL_WA_SIZE, NORMALPRIO);
	if (!_chShellThread) {
		return false;
	}

	return true;
}

void cmdDestroy(void)
{
	chThdTerminate(_chShellThread);

	_chShellThread = 0;
}

void cmdPrintf(const char* msg)
{
	// Let's pretend that we print through the shell
	if (!_chShellThread) {
		return;
	}

	chprintf((BaseSequentialStream*)&SDU1, msg);
}

void cmdManage(void){
    if (!_chShellThread && (SDU1.config->usbp->state == USB_ACTIVE))
            _chShellThread = shellCreate(&_shell_cfg1, SHELL_WA_SIZE, NORMALPRIO);
        else if (chThdTerminatedX(_chShellThread)) {
            chThdRelease(_chShellThread);    /* Recovers memory of the previous shell.   */
            _chShellThread = 0;           /* Triggers spawning of a new shell.        */
        }
}
