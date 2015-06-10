###############################################################################
#                                                                             #
# Logswan (c) by Frederic Cambus 2015                                         #
# https://github.com/fcambus/logswan                                          #
#                                                                             #
# Last Updated: 2015/06/07                                                    #
#                                                                             # 
###############################################################################

cmake_minimum_required (VERSION 2.6)

project (logswan C)

add_definitions(-Wall -Wextra -Werror -std=c99 -pedantic)

add_executable(logswan src/logswan.c)