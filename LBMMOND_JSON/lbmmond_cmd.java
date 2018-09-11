import com.latencybusters.lbm.*;

// See https://communities.informatica.com/infakb/faq/5/Pages/80008.aspx
import org.openmdx.uses.gnu.getopt.*;

/*
  Copyright (c) 2005-2013 Informatica Corporation  Permission is granted to licensees to use
  or alter this software for any purpose, including commercial applications,
  according to the terms laid out in the Software License Agreement.

  This source code example is provided by Informatica for educational
  and evaluation purposes only.

  THE SOFTWARE IS PROVIDED "AS IS" AND INFORMATICA DISCLAIMS ALL WARRANTIES 
  EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION, ANY IMPLIED WARRANTIES OF 
  NON-INFRINGEMENT, MERCHANTABILITY OR FITNESS FOR A PARTICULAR 
  PURPOSE.  INFORMATICA DOES NOT WARRANT THAT USE OF THE SOFTWARE WILL BE 
  UNINTERRUPTED OR ERROR-FREE.  INFORMATICA SHALL NOT, UNDER ANY CIRCUMSTANCES, BE 
  LIABLE TO LICENSEE FOR LOST PROFITS, CONSEQUENTIAL, INCIDENTAL, SPECIAL OR 
  INDIRECT DAMAGES ARISING OUT OF OR RELATED TO THIS AGREEMENT OR THE 
  TRANSACTIONS CONTEMPLATED HEREUNDER, EVEN IF INFORMATICA HAS BEEN APPRISED OF 
  THE LIKELIHOOD OF SUCH DAMAGES.

  NOTES:
  This is a sample java implementation of the umedcmd.c and tnwgdcmd.c sample applications. 
  The main purpose it to send unicast immediate command messages to an umestore or tnwgd
  daemon to request monitoring statistics data.
  A timer is also created that checks for a response from the daemon to the executed command,
  and a warning is printed no response from the daemon was received.
  
*/

import java.io.*;
import java.util.Scanner;


class lbmmond_cmd
{
	private static String purpose = "Purpose: Request data that is monitoring from umestored and tnwgd daemons";
	private static String usage =
	  "Usage: lbmmond_cmd [options]\n"
	+ "Available options:\n"
	+ "  -c filename =         Use LBM flat-file configuration filename.\n"
	+ "                        Multiple config files are allowed.\n"
	+ "                        Example:  '-c file1.cfg -c file2.cfg'\n"
	+ "  -C filename =         read Java Properties file for context config\n"
	+ "  -L, --linger=NUM =    Linger for NUM seconds before closing context\n"
	+ "  -x, --xml-config =    XML configuration file for context \n"
	+ "  -a, --xml-appname =   XML Application name in the XML configuration file\n"
	+ "  -T, --target =        Request TCP port Target to the UIM e.g: [TCP:domain:ip:port],[TCP:ip:port],[SOURCE:source-string] \n"
	+ "  -t, --timeout =       Timeout to wait for Request (defaults to 5 seconds) \n"
	+ "\n"
	+ "Example:\n\t java -cp $CP  lbmmond_cmd -T\"TCP:10.29.2.146:15551\" \n"
	;

	private static String dro_commands = 
	"************************************************************************************\n"
	+"* DRO commands:                                                                    \n"
	+"*   set publishing interval: \t(0-N = interval in seconds);                        \n"
	+"*                            \tri 0-N       (routing info);                        \n"
	+"*                            \tgcfg 0-N     (gateway config);                      \n"
	+"*         [\"portal name\"]  \tpcfg 0-N     (portal config);                       \n"
	+"*         [\"portal name\"]  \tpstat 0-N    (portal stats);                        \n"
	+"*                            \tmallinfo 0-N (malloc info);                         \n"
	+"*                                                                                  \n"
	+"*   snapshot all groups (and all portals) : snap;                                  \n"
	+"*   snapshot single group: snap (ri|gcfg|pcfg|pstat|mallinfo);                     \n"
	+"*   snapshot single portal: \"portal name\" snap pcfg|pstat;                       \n"
	+"*   Print the current version of the monitor: version;                             \n"
	+"***********************************************************************************\n"
	;

	private static String umestored_commands = 
	"*************************************************************************************\n"
	+"* Store commands:                                                                   \n"
	+"*   report store dmon version: version;                                             \n"
	+"*   set publishing interval: memory 0-N;                                            \n"
	+"*                            [\"store name\"] src 0-N;                              \n"
	+"*                            [\"store name\"] rcv 0-N;                              \n"
	+"*                            [\"store name\"] disk 0-N;                             \n"
	+"*                            [\"store name\"] store 0-N;                            \n"
	+"*                            [\"store name\"] config 0-N;                           \n"
	+"*       snapshot all groups: [\"store name\"] snap memory|src|rcv|disk|store|config; \n"
	+"*                                                                                   \n"
	+"*  Example: \"Example_Store_1\" snap;\"Example_Store_1\" disk 5;                    \n"
	+"************************************************************************************\n"
	;

	public static void main(String[] args)
	{
		@SuppressWarnings("unused")
		lbmmond_cmd trreqapp = new lbmmond_cmd(args);
	}
	
	int linger = 5;
	int duration = -1;
	int interval = -1;
	short flags = 0;
	long timeout = 5000;
	String cconffname = null;
	String xml_filename = null;
	String xml_appname = "";
	String target = null;

	private void process_cmdline(String[] args)
	{
		LongOpt[] longopts = new LongOpt[3];

		longopts[0] = new LongOpt("cfg", LongOpt.REQUIRED_ARGUMENT, null, 'c');
		longopts[1] = new LongOpt("xml-config", LongOpt.REQUIRED_ARGUMENT, null, 'x');
		longopts[2] = new LongOpt("xml-appname", LongOpt.REQUIRED_ARGUMENT, null, 'a');
		Getopt gopt = new Getopt("lbmmond_cmd", args, "c:x:a:T:h", longopts);
		int c = -1;
		boolean error = false;

		while ((c = gopt.getopt()) != -1)
		{
			try
			{
				switch (c)
				{
					case 'c':
						try 
						{
							LBM.setConfiguration(gopt.getOptarg());
						}
						catch (LBMException ex) 
						{
							System.err.println("Error setting LBM configuration: " + ex.toString());
							System.exit(1);
						}
						break;
					case 'C':
						cconffname = gopt.getOptarg();
						break;
					case 'x':
						xml_filename = gopt.getOptarg();
						break;
					case 'a':
						xml_appname = gopt.getOptarg();
						break;
					case 'h':
						print_help_exit(0);
						break;
					case 'T':
						target = gopt.getOptarg();
						break;
					case 't':
						timeout = Integer.parseInt(gopt.getOptarg());
						break;
					default:
						error = true;
						break;
				}
				if (error)
					break;
			}
			catch (Exception e)
			{
				/* type conversion exception */
				System.err.println("lbmmond_cmd: error\n" + e);
				print_help_exit(1);
			}
		}
		
		if (error)
		{	
			/* An error occurred processing the command line - print help and exit */
			print_help_exit(1);
		}
	}
	
	private static void print_help_exit(int exit_value)
	{
		System.err.println(LBM.version());
		System.err.println(purpose);
		System.err.println(usage);
		System.exit(exit_value);
	}

	private lbmmond_cmd(String[] args)
	{
		LBM lbm = null;
		LBMContextAttributes cattr = null;
		try
		{
			lbm = new LBM();
		}
		catch (LBMException ex)
		{
			System.err.println("Error initializing LBM: " + ex.toString());
			System.exit(1);
		}
		org.apache.log4j.Logger logger;
		logger = org.apache.log4j.Logger.getLogger("lbmmond_cmd");
		org.apache.log4j.BasicConfigurator.configure();
		log4jLogger lbmlogger = new log4jLogger(logger);
		lbm.setLogger(lbmlogger);

		process_cmdline(args);

		try 
		{
			cattr = new LBMContextAttributes();
		}
		catch (LBMException ex)
		{
			System.err.println("Error creating context attributes: " + ex.toString());
			System.exit(1);	
		}

		if (cconffname != null)
		{
			try 
			{
				FileInputStream f = new FileInputStream(cconffname);
				cattr.load(f);
			}
			catch (IOException e)
			{
				System.err.println(e.toString());
				System.exit(1);
			}
		}
		if (cattr.size() > 0)
			cattr.list(System.out);
		LBMContext ctx = null;
		try
		{
			ctx = new LBMContext(cattr);
		}
		catch (LBMException ex)
		{
			System.err.println("Error creating context: " + ex.toString());
			System.exit(1);
		}
                if( xml_filename != null ){
			try
			{
				LBM.setConfigurationXml(xml_filename, xml_appname);
			}
			catch (LBMException ex)
			{
				System.err.println("Could not set XML configuration for " + xml_filename + ":" + xml_appname + "; " + ex.toString());
			}
		}
		if( target == null){
			System.err.println("Target for the command is needed ");
			System.exit(1);
		}

		int stop = 0;
		String dCmd = null;
		Scanner input = new Scanner(System.in);
		char[] targetCharArray = target.toCharArray();
		byte[] dCmdBytes=null;
		input.useDelimiter(";");
		while (stop == 0){

			
			System.out.println(umestored_commands + dro_commands);
			System.out.println("Type in command(s) delimited by ';' OR 'q;' to quit: ");
			dCmd = input.next(); 
			dCmd = dCmd.replaceAll("\\n", ""); /* Get rid of carriage return */
			dCmd = dCmd.replaceAll("\\r", "");
			dCmdBytes = dCmd.getBytes();

			System.err.println("Target: " + target + " Command: " + dCmd + ";");
			if (dCmd.equalsIgnoreCase("q")){
				stop = 1;
			} else {
				try {
					ReqTimer Reqtimeout = new ReqTimer(ctx, timeout, dCmd); 
					LBMreqCB reqcb = new LBMreqCB(Reqtimeout);
					/* Requests are closed when a response is received. */
					LBMRequest req = ctx.send(target, null, dCmdBytes, dCmdBytes.length, reqcb, Reqtimeout, null, 1);
				}
				catch (LBMException ex)
				{
					System.err.println("Error sending command to daemon: " + ex.toString());
					System.exit(1);
				}
			}
		}

		input.close();
		ctx.close();
		System.err.println("Bye ...");
	}
}

class LBMreqCB implements LBMResponseCallback
{
        ReqTimer _reqtimer;
        public LBMreqCB(ReqTimer reqtimer) 
        {
		_reqtimer = reqtimer;
	}
	public int onResponse(Object cbArg, LBMRequest req, LBMMessage msg)
	{
                switch (msg.type())
                {
			case LBM.MSG_RESPONSE:
				System.out.println("\nResponse ["
							+ msg.source()
							+ "]["
							+ msg.sequenceNumber()
							+ "], "
							+ msg.dataLength()
							+ " bytes: "
							+ msg.dataString());
				try 
				{
					_reqtimer.cancel();
				}
				catch (LBMException ex)
				{
					System.err.println("WARNING: Error cancelling timer: " + ex.toString());
				}
				System.out.println("\nType in command delimited by ';' OR 'q' to quit: ");
			break;
			default:
				System.out.println("Unknown message type "
							+ msg.type()
							+ "["
							+ msg.source()
							+"]");
			break;
                }
                System.out.flush();
                msg.dispose();
		try 
		{
			req.close();
		}
		catch (LBMException ex)
		{
			System.err.println("WARNING: Error cancelling timer: " + ex.toString());
		}
                return 0;
        }
}



class ReqTimer extends LBMTimer
{
	String _command;

        public ReqTimer(LBMContext ctx, long tmo, String command) throws LBMException
        {
                super(ctx, tmo, null);
		_command = command;
	}
        private void onExpiration()
        {      
		System.err.println("\n WARNING! ********** Command timed out ********:" + _command);    
		System.out.println("\nType in command delimited by ';' OR 'q;' to quit: ");
        }
}
