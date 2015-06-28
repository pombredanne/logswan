/*****************************************************************************/
/*                                                                           */
/* Logswan (c) by Frederic Cambus 2015                                       */
/* https://github.com/fcambus/logswan                                        */
/*                                                                           */
/* Created:      2015/05/31                                                  */
/* Last Updated: 2015/06/24                                                  */
/*                                                                           */
/* Logswan is released under the BSD 3-Clause license.                       */
/* See LICENSE file for details.                                             */
/*                                                                           */
/*****************************************************************************/

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <getopt.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifndef HAVE_STRTONUM
#include "../compat/strtonum.h"
#endif

#include <GeoIP.h>

#include "output.h"
#include "parse.h"
#include "results.h"

#define VERSION "Logswan"
#define LINE_MAX_LENGTH 4096

GeoIP *geoip;

clock_t begin, end;

char lineBuffer[LINE_MAX_LENGTH];

Results results;
struct date parsedDate;
struct logLine parsedLine;

struct sockaddr_in ipv4;
struct sockaddr_in6 ipv6;
int isIPv4, isIPv6;

uint64_t bandwidth;
int statusCode;
int hour;

struct stat logFileSize;
FILE *logFile, *jsonFile;

const char *errstr;

int getoptFlag;

int main (int argc, char *argv[]) {
	printf("-------------------------------------------------------------------------------\n" \
	       "                      Logswan (c) by Frederic Cambus 2015                      \n" \
	       "-------------------------------------------------------------------------------\n\n");

	if (argc != 2) {
		printf("ERROR : No input file specified.\n");
		return EXIT_FAILURE;
	}

	while ((getoptFlag = getopt(argc, argv, "v")) != -1) {
		switch(getoptFlag) {
		case 'v':
			printf(VERSION);
			return 0;
		}
	}

	/* Starting timer */
	begin = clock();

	/* Initializing GeoIP */
	geoip = GeoIP_open("GeoIP.dat", GEOIP_MEMORY_CACHE);

	/* Get log file size */
	stat(argv[1], &logFileSize);
	results.fileSize = (uint64_t)logFileSize.st_size;

	printf("Processing file : %s\n\n", argv[1]);

	logFile = fopen(argv[1], "r");
	if (!logFile) {
		perror("Can't open log file");
		return EXIT_FAILURE;
	}

	/* Create output file */
	int outputLen = strlen(argv[1]) + 6;
	char *outputFile = malloc(outputLen);
	snprintf(outputFile, outputLen, "%s%s", argv[1], ".json");

	jsonFile = fopen(outputFile, "w");
	if (!jsonFile) {
		perror("Can't create output file");
		return EXIT_FAILURE;
	}

	while (fgets(lineBuffer, LINE_MAX_LENGTH, logFile) != NULL) {
		/* Parse and tokenize line */
		parseLine(&parsedLine, lineBuffer);

		/* Detect if remote host is IPv4 or IPv6 */
		if (parsedLine.remoteHost) { /* Do not feed NULL tokens to inet_pton */
			isIPv4 = inet_pton(AF_INET, parsedLine.remoteHost, &(ipv4.sin_addr));
			isIPv6 = inet_pton(AF_INET6, parsedLine.remoteHost, &(ipv6.sin6_addr));
		}

		if (isIPv4 || isIPv6) {
			/* Increment countries array */
			if (geoip && isIPv4) {
				results.countries[GeoIP_id_by_addr(geoip, parsedLine.remoteHost)]++;
			}

			/* Hourly distribution */
			parseDate(&parsedDate, parsedLine.date);

			if (parsedDate.hour) {
				hour = strtonum(parsedDate.hour, 0, 23, &errstr);

				if (!errstr) {
					results.hours[hour] ++;
				}
			}

			/* Count HTTP status codes occurences */
			if (parsedLine.statusCode) {
				statusCode = strtonum(parsedLine.statusCode, 0, STATUS_CODE_MAX-1, &errstr);

				if (!errstr) {
					results.httpStatus[statusCode] ++;
				}
			}

			/* Increment bandwidth usage */
			if (parsedLine.objectSize) {
				bandwidth = strtonum(parsedLine.objectSize, 0, INT64_MAX, &errstr);

				if (!errstr) {					
					results.bandwidth += bandwidth;
				}
			}

			/* Increment hits counter */
			results.hitsIPv4 += isIPv4;
			results.hitsIPv6 += isIPv6;
			results.hits++;
		} else {
			/* Invalid line */

			results.invalidLines++;
		}

		/* Increment processed lines counter */
		results.processedLines++;
	}

	/* Stopping timer */
	end = clock();
	results.runtime = (double)(end - begin) / CLOCKS_PER_SEC;

	/* Generate timestamp */
	time_t now = time(NULL);
	strftime(results.timeStamp, 20, "%Y-%m-%d %H:%M:%S", localtime(&now));

	/* Printing results */
	printf("Processed %" PRIu64 " lines in %f seconds\n", results.processedLines, results.runtime);
	fclose(logFile);

	fputs(output(results), jsonFile);
	printf("Created file : %s\n", outputFile);
	fclose(jsonFile);

	return EXIT_SUCCESS;
}
