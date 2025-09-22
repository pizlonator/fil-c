/****
** 04/21/96
** hacked even more, replaced gets() with something slightly harder to buffer
** overflow. Added support for setting a default quota on new account, with
** edquota -p. Other cleanups for security, I let some users run adduser suid
** root to add new accounts. (overflow checks, clobber environment, valid 
** shell checks, restrictions on gid + home dir settings).

** Added max. username length. Used syslog() a bit for important events. 
** Support to immediately expire account with passwd -e.

** Called it version 2.0! Because I felt like it!

** -- Chris, chris@ferret.lmh.ox.ac.uk

** 03/17/96
** hacked a bit more, removed unused code, cleaned up for gcc -Wall.
** --marekm
**
** 02/26/96
** modified to call shadow utils (useradd,chage,passwd) on shadowed 
** systems - Cristian Gafton, gafton@sorosis.ro
**
** 6/27/95
** shadow-adduser 1.4:
**
** now it copies the /etc/skel dir into the person's dir, 
** makes the mail folders, changed some defaults and made a 'make 
** install' just for the hell of it.
**
** Greg Gallagher
** CIN.Net
**
** 1/28/95
** shadow-adduser 1.3:
** 
** Basically a bug-fix on my additions in 1.2.  Thanks to Terry Stewart 
** (stew@texas.net) for pointing out one of the many idiotic bugs I introduced.
** It was such a stupid bug that I would have never seen it myself.
**
**                                Brandon
*****
** 01/27/95
** 
** shadow-adduser 1.2:
** I took the C source from adduser-shadow (credits are below) and made
** it a little more worthwhile.  Many small changes... Here's
** the ones I can remember:
** 
** Removed support for non-shadowed systems (if you don't have shadow,
**     use the original adduser, don't get this shadow version!)
** Added support for the correct /etc/shadow fields (Min days before
**     password change, max days before password change, Warning days,
**     and how many days from expiry date does the account go invalid)
**     The previous version just left all of those fields blank.
**     There is still one field left (expiry date for the account, period)
**     which I have left blank because I do not use it and didn't want to
**     spend any more time on this.  I'm sure someone will put it in and
**     tack another plethora of credits on here. :)
** Added in the password date field, which should always reflect the last
**     date the password was changed, for expiry purposes.  "passwd" always
**     updates this field, so the adduser program should set it up right
**     initially (or a user could keep their initial password forever ;)
**     The number is in days since Jan 1st, 1970.
**
**                       Have fun with it, and someone please make
**                       a real version(this is still just a hack)
**                       for us all to use (and Email it to me???)
**
**                               Brandon
**                                  photon@usis.com
**
***** 
** adduser 1.0: add a new user account (For systems not using shadow)
** With a nice little interface and a will to do all the work for you.
**
** Craig Hagan
** hagan@opine.cs.umass.edu
**
** Modified to really work, look clean, and find unused uid by Chris Cappuccio
** chris@slinky.cs.umass.edu
**
*****
**
** 01/19/95
**
** FURTHER modifications to enable shadow passwd support (kludged, but
** no more so than the original)  by Dan Crowson - dcrowson@mo.net
**
** Search on DAN for all changes...
**
*****
**
** cc -O -o adduser adduser.c
** Use gcc if you have it... (political reasons beyond my control) (chris)
**
** I've gotten this program to work with success under Linux (without
** shadow) and SunOS 4.1.3. I would assume it should work pretty well
** on any system that uses no shadow. (chris)
**
** If you have no crypt() then try
** cc -DNO_CRYPT -O -o adduser adduser.c xfdes.c
** I'm not sure how login operates with no crypt()... I guess
** the same way we're doing it here.
*/

#include <unistd.h>
#include <stdlib.h>
#include <pwd.h>
#include <grp.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/timeb.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <syslog.h>

#define IMMEDIATE_CHANGE	/* Expire newly created password, must be changed
				 * immediately upon next login */
#define HAVE_QUOTAS		/* Obvious */
#define EXPIRE_VALS_SET		/* If defined, 'normal' users can't change 
				 * password expiry values (if running suid root) */

#define HAVE_GETUSERSHELL	/* FIXME: Isn't this defined in config.h too? */
#define LOGGING			/* If we want to log various things to syslog */
#define MAX_USRNAME  8		/* Longer usernames seem to work on my system....
				 * But they're probably a poor idea */


#define DEFAULT_SHELL	"/bin/bash"	/* because BASH is your friend */
#define DEFAULT_HOME	"/home"
#define USERADD_PATH	"/usr/sbin/useradd"
#define CHAGE_PATH	"/usr/bin/chage"
#define PASSWD_PATH	"/usr/bin/passwd"
#define EDQUOTA_PATH	"/usr/sbin/edquota"
#define QUOTA_DEFAULT	"defuser"
#define DEFAULT_GROUP	100

#define DEFAULT_MIN_PASS 0
#define DEFAULT_MAX_PASS 100
#define DEFAULT_WARN_PASS 14
#define DEFAULT_USER_DIE 366

void safeget (char *, int);

void 
main (void)
{
  char foo[32];
  char usrname[32], person[32], dir[32], shell[32];
  unsigned int group, min_pass, max_pass, warn_pass, user_die;
  /* the group and uid of the new user */
  int bad = 0, done = 0, correct = 0, olduid;
  char cmd[255];
  struct group *grp;

  /* flags, in order:
   * bad to see if the username is in /etc/passwd, or if strange stuff has
   * been typed if the user might be put in group 0
   * done allows the program to exit when a user has been added
   * correct loops until a username is found that isn't in /etc/passwd
   */

  /* The real program starts HERE! */

  if (geteuid () != 0)
    {
      printf ("It seems you don't have access to add a new user.  Try\n");
      printf ("logging in as root or su root to gain superuser access.\n");
      exit (1);
    }

  /* Sanity checks
   */

#ifdef LOGGING
  openlog ("adduser", LOG_PID | LOG_CONS | LOG_NOWAIT, LOG_AUTH);
  syslog (LOG_INFO, "invoked by user %s\n", getpwuid (getuid ())->pw_name);
#endif

  if (!(grp = getgrgid (DEFAULT_GROUP)))
    {
      printf ("Error: the default group %d does not exist on this system!\n",
	      DEFAULT_GROUP);
      printf ("adduser must be recompiled.\n");
#ifdef LOGGING
      syslog (LOG_ERR, "warning: failed. no such default group\n");
      closelog ();
#endif
      exit (1);
    };

  while (!correct)
    {				/* loop until a "good" usrname is chosen */
      while (!done)
	{
	  printf ("\nLogin to add (^C to quit): ");
	  fflush (stdout);

	  safeget (usrname, sizeof (usrname));

	  if (!strlen (usrname))
	    {
	      printf ("Empty input.\n");
	      done = 0;
	      continue;
	    };

	  /* what I saw here before made me think maybe I was running DOS */
	  /* might this be a solution? (chris) */
	  if (strlen (usrname) > MAX_USRNAME)
	    {
	      printf ("That name is longer than the maximum of %d characters. Choose another.\n", MAX_USRNAME);
	      done = 0;
	    }
	  else if (getpwnam (usrname) != NULL)
	    {
	      printf ("That name is in use, choose another.\n");
	      done = 0;
	    }
	  else if (strchr (usrname, ' ') != NULL)
	    {
	      printf ("No spaces in username!!\n");
	      done = 0;
	    }
	  else
	    done = 1;
	};			/* done, we have a valid new user name */

      /* all set, get the rest of the stuff */
      printf ("\nEditing information for new user [%s]\n", usrname);

      printf ("\nFull Name [%s]: ", usrname);
      fflush (stdout);
      safeget (person, sizeof (person));
      if (!strlen (person))
	{
	  bzero (person, sizeof (person));
	  strcpy (person, usrname);
	};

      if (getuid () == 0)
	{
	  do
	    {
	      bad = 0;
	      printf ("GID [%d]: ", DEFAULT_GROUP);
	      fflush (stdout);
	      safeget (foo, sizeof (foo));
	      if (!strlen (foo))
		group = DEFAULT_GROUP;
	      else if (isdigit (*foo))
		{
		  group = atoi (foo);
		  if (!(grp = getgrgid (group)))
		    {
		      printf ("unknown gid %s\n", foo);
		      group = DEFAULT_GROUP;
		      bad = 1;
		    };
		}
	      else if ((grp = getgrnam (foo)))
		group = grp->gr_gid;
	      else
		{
		  printf ("unknown group %s\n", foo);
		  group = DEFAULT_GROUP;
		  bad = 1;
		}
	      if (group == 0)
		{		/* You're not allowed to make root group users! */
		  printf ("Creation of root group users not allowed (must be done by hand)\n");
		  group = DEFAULT_GROUP;
		  bad = 1;
		};
	    }
	  while (bad);
	}
      else
	{
	  printf ("Group will be default of: %d\n", DEFAULT_GROUP);
	  group = DEFAULT_GROUP;
	}

      if (getuid () == 0)
	{
	  printf ("\nIf home dir ends with a / then '%s' will be appended to it\n", usrname);
	  printf ("Home Directory [%s/%s]: ", DEFAULT_HOME, usrname);
	  fflush (stdout);
	  safeget (dir, sizeof (dir));
	  if (!strlen (dir))
	    {			/* hit return */
	      sprintf (dir, "%s/%s", DEFAULT_HOME, usrname);
	    }
	  else if (dir[strlen (dir) - 1] == '/')
	    sprintf (dir+strlen(dir), "%s", usrname);
	}
      else
	{
	  printf ("\nHome directory will be %s/%s\n", DEFAULT_HOME, usrname);
	  sprintf (dir, "%s/%s", DEFAULT_HOME, usrname);
	}

      printf ("\nShell [%s]: ", DEFAULT_SHELL);
      fflush (stdout);
      safeget (shell, sizeof (shell));
      if (!strlen (shell))
	sprintf (shell, "%s", DEFAULT_SHELL);
      else
	{
	  char *sh;
	  int ok = 0;
#ifdef HAVE_GETUSERSHELL
	  setusershell ();
	  while ((sh = getusershell ()) != NULL)
	    if (!strcmp (shell, sh))
	      ok = 1;
	  endusershell ();
#endif
	  if (!ok)
	    {
	      if (getuid () == 0)
		printf ("Warning: root allowed non standard shell\n");
	      else
		{
		  printf ("Shell NOT in /etc/shells, DEFAULT used\n");
		  sprintf (shell, "%s", DEFAULT_SHELL);
		}
	    }
	}

#ifdef EXPIRE_VALS_SET
      if (getuid () == 0)
	{
#endif
	  printf ("\nMin. Password Change Days [%d]: ", DEFAULT_MIN_PASS);
	  fflush (stdout);
	  safeget (foo, sizeof (foo));
	  if (strlen (foo) > 1)
	    min_pass = DEFAULT_MIN_PASS;
	  else
	    min_pass = atoi (foo);

	  printf ("Max. Password Change Days [%d]: ", DEFAULT_MAX_PASS);
	  fflush (stdout);
	  safeget (foo, sizeof (foo));
	  if (strlen (foo) > 1)
	    max_pass = atoi (foo);
	  else
	    max_pass = DEFAULT_MAX_PASS;

	  printf ("Password Warning Days [%d]: ", DEFAULT_WARN_PASS);
	  fflush (stdout);
	  safeget (foo, sizeof (foo));
	  warn_pass = atoi (foo);
	  if (warn_pass == 0)

	    warn_pass = DEFAULT_WARN_PASS;

	  printf ("Days after Password Expiry for Account Locking [%d]: ", DEFAULT_USER_DIE);
	  fflush (stdout);
	  safeget (foo, sizeof (foo));
	  user_die = atoi (foo);
	  if (user_die == 0)
	    user_die = DEFAULT_USER_DIE;

#ifdef EXPIRE_VALS_SET
	}
      else
	{
	  printf ("\nSorry, account expiry values are set.\n");
	  user_die = DEFAULT_USER_DIE;
	  warn_pass = DEFAULT_WARN_PASS;
	  max_pass = DEFAULT_MAX_PASS;
	  min_pass = DEFAULT_MIN_PASS;
	}
#endif

      printf ("\nInformation for new user [%s] [%s]:\n", usrname, person);
      printf ("Home directory: [%s] Shell: [%s]\n", dir, shell);
      printf ("GID: [%d]\n", group);
      printf ("MinPass: [%d] MaxPass: [%d] WarnPass: [%d] UserExpire: [%d]\n",
	      min_pass, max_pass, warn_pass, user_die);
      printf ("\nIs this correct? [y/N]: ");
      fflush (stdout);
      safeget (foo, sizeof (foo));

      done = bad = correct = (foo[0] == 'y' || foo[0] == 'Y');

      if (bad != 1)
	printf ("\nUser [%s] not added\n", usrname);
    }

  /* Clobber the environment, I run this suid root sometimes to let 
   * non root privileged accounts add users --chris */

  *environ = NULL;

  bzero (cmd, sizeof (cmd));
  sprintf (cmd, "%s -g %d -d %s -s %s -c \"%s\" -m -k /etc/skel %s",
	   USERADD_PATH, group, dir, shell, person, usrname);
  printf ("Calling useradd to add new user:\n%s\n", cmd);
  if (system (cmd))
    {
      printf ("User add failed!\n");
#ifdef LOGGING
      syslog (LOG_ERR, "could not add new user\n");
      closelog ();
#endif
      exit (errno);
    };

  olduid = getuid ();	/* chage, passwd, edquota etc. require ruid = root
			 */
  setuid (0);

  bzero (cmd, sizeof (cmd));

  /* Chage runs suid root. => we need ruid root to run it with
   * anything other than chage -l
   */

  sprintf (cmd, "%s -m %d -M %d -W %d -I %d %s", CHAGE_PATH,
	   min_pass, max_pass, warn_pass, user_die, usrname);
  printf ("%s\n", cmd);
  if (system (cmd))
    {
      printf ("There was an error setting password expire values\n");
#ifdef LOGGING
      syslog (LOG_ERR, "password expire values could not be set\n");
#endif
    };

  /* I want to add a user completely with one easy command --chris */

#ifdef HAVE_QUOTAS
  bzero (cmd, sizeof (cmd));
  sprintf (cmd, "%s -p %s -u %s", EDQUOTA_PATH, QUOTA_DEFAULT, usrname);
  printf ("%s\n", cmd);
  if (system (cmd))
    {
      printf ("\nWarning: error setting quota\n");
#ifdef LOGGING
      syslog (LOG_ERR, "warning: account created but NO quotas set!\n");
#endif /* LOGGING */
    }
  else
    printf ("\nDefault quota set.\n");
#endif /* HAVE_QUOTAS */

  bzero (cmd, sizeof (cmd));
  sprintf (cmd, "%s %s", PASSWD_PATH, usrname);
  if (system (cmd))
    {
      printf ("\nWarning: error setting password\n");
#ifdef LOGGING
      syslog (LOG_ERR, "warning: password set failed!\n");
#endif
    }
#ifdef IMMEDIATE_CHANGE
  bzero (cmd, sizeof (cmd));
  sprintf (cmd, "%s -e %s", PASSWD_PATH, usrname);
  if (system (cmd))
    {
      printf ("\nWarning: error expiring password\n");
#ifdef LOGGING
      syslog (LOG_ERR, "warning: password expire failed!\n");
#endif /* LOGGING */
    }
#endif /* IMMEDIATE_CHANGE */

  setuid (olduid);

#ifdef LOGGING
  closelog ();
#endif

  printf ("\nDone.\n");
}

void 
safeget (char *buf, int maxlen)
{
  int c, i = 0, bad = 0;
  char *bstart = buf;
  while ((c = getc (stdin)) != EOF && (c != '\n') && (++i < maxlen))
    {
      bad = (!isalnum (c) && (c != '_') && (c != ' '));
      *(buf++) = c;
    }
  *buf = '\0';

  if (bad)
    {
      printf ("\nString contained banned character. Please stick to alphanumerics.\n");
      *bstart = '\0';
    }
}

