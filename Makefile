# name of your application
APPLICATION = oss7modem-test

# If no BOARD is found in the environment, use this default:
BOARD ?= native

# This has to be the absolute path to the RIOT base directory:
RIOTBASE ?= ../RIOT

# Comment this out to disable code in RIOT that does safety checking
# which is not needed in a production environment but helps in the
# development process:
DEVELHELP ?= 1

# Change this to 0 show compiler invocation lines by default:
QUIET ?= 1

# do not fail build on warning
# some code in ALP not compliant C99
#WERROR = 0

USEMODULE += shell
USEMODULE += shell_commands
USEMODULE += ps # debug threads

USEMODULE += auto_init # voor timer init
USEMODULE += xtimer

#Should be automatic dependency
EXTERNAL_MODULE_DIRS += $(RIOTPROJECT)/drivers/fifo
USEMODULE += fifo

EXTERNAL_MODULE_DIRS += $(RIOTPROJECT)/drivers/oss7modem
USEMODULE += oss7modem

INCLUDES += -I$(RIOTPROJECT)/drivers/include

include $(RIOTBASE)/Makefile.include
