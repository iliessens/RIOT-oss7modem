# name of your application
APPLICATION = oss7modem-test

# If no BOARD is found in the environment, use this default:
BOARD ?= native

# This has to be the absolute path to the RIOT base directory:
RIOTBASE ?= $(HOME)/RIOT

# Comment this out to disable code in RIOT that does safety checking
# which is not needed in a production environment but helps in the
# development process:
DEVELHELP ?= 1

# Change this to 0 show compiler invocation lines by default:
QUIET ?= 1

# do not fail build on warning
# some code in ALP not compliant C99
WERROR = 0

EXTERNAL_MODULE_DIRS += $(RIOTPROJECT)/drivers/oss7modem
USEMODULE += oss7modem
#extra dependency path
EXTERNAL_MODULE_DIRS += $(RIOTPROJECT)/drivers/fifo

INCLUDES += -I$(RIOTPROJECT)/drivers/include

include $(RIOTBASE)/Makefile.include