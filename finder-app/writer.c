#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>

int main(int argc, char *argv[])
{
    FILE *fp;
    openlog(NULL, 0, LOG_USER);

    if (argc ==3)
    {
        fp = fopen(argv[1], "w");
        if (fp == NULL)
        {
            syslog(LOG_ERR, "Error opening file %s: %s\n", argv[1], strerror(errno));
            fprintf(stderr, "Error opening file %s: %s\n", argv[1], strerror(errno));
            return 1;
        }
        else
        {
            syslog(LOG_DEBUG, "Writing %s to %s\n", argv[2], argv[1]);
            fprintf(fp, "%s\n", argv[2]);

            fclose(fp);
            closelog();
            return 0;
        }
    }
    else
    {
        fprintf(stderr, "Invalig number of arguments: %d\n", argc);
        syslog(LOG_ERR,"Invalig number of arguments: %d\n", argc);
        return 1;
    }
}
