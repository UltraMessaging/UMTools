import com.latencybusters.lbm.*;
import com.latencybusters.lbm.*;
import com.latencybusters.lbm.UMSMonProtos.*;
import com.latencybusters.lbm.UMSMonProtos.UMSMonMsg.*;
import com.latencybusters.lbm.UMSMonProtos.UMSMonMsg.Stats.*;
import com.latencybusters.lbm.UMSMonProtos.UMSMonMsg.Events.*;
import com.latencybusters.lbm.UMMonAttributesProtos.*;
import com.latencybusters.lbm.UMPMonProtos.*;
import com.latencybusters.lbm.DROMonProtos.*;
import com.google.protobuf.TextFormat;
import java.nio.ByteBuffer;
import java.util.Date;

// See https://communities.informatica.com/infakb/faq/5/Pages/80008.aspx
import org.openmdx.uses.gnu.getopt.*;

/*
  (C) Copyright 2005,2022 Informatica LLC  Permission is granted to licensees to use
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
*/

class lbmmon
{
	private static String purpose = "Purpose: Example LBM statistics monitoring application.";
	private static String usage =
"Usage: lbmmon [options]\n"+ 
"Available options:\n"+ 
"  -h, --help                 help\n"+ 
"  -t, --transport TRANS      use transport module TRANS\n"+ 
"                             TRANS may be `lbm', `udp', or `lbmsnmp', default is `lbm'\n"+ 
"      --transport-opts OPTS  use OPTS as transport module options\n"+ 
"  -f, --format FMT           use format module FMT\n"+ 
"                             FMT may be `csv' or `pb', default is `csv'\n"+ 
"      --format-opts OPTS     use OPTS as format module options\n"+ 
"\n"+ 
"Transport and format options are passed as name=value pairs, separated by a semicolon.\n"+ 
"\n"+ 
"LBM transport options:\n"+
"  config=FILE            use LBM configuration file FILE\n"+
"  topic=TOPIC            receive statistics on topic TOPIC\n"+ 
"                         default is /29west/statistics\n"+ 
"  wctopic=PATTERN        receive statistics on wildcard topic PATTERN\n"+ 
"\n"+ 
"UDP transport options:\n"+ 
"  port=NUM               receive on UDP port NUM\n"+ 
"  interface=IP           receive multicast on interface IP\n"+ 
"  mcgroup=GRP            receive on multicast group GRP\n"+ 
"\n"+ 
"LBMSNMP transport options:\n"+ 
"  config=FILE            use LBM configuration file FILE\n"+ 
"  topic=TOPIC            receive statistics on topic TOPIC\n"+ 
"                         default is /29west/statistics\n"+ 
"  wctopic=PATTERN        receive statistics on wildcard topic PATTERN\n"+ 
"\n"+ 
"CSV format options:\n"+ 
"  separator=CHAR         separate CSV fields with character CHAR\n"+ 
"                         defaults to `,'\n"+
"  passthrough=VAL        VAL may be `off', `on' or `convert'\n"+
"                         defaults to `off'\n"+
"PB format options:\n"+
"  passthrough=VAL        VAL may be `off', `on' or `convert'\n"+
"                         defaults to `off'\n"
;


    public static void main(String[] args)
	{
		LBM lbm = null;
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
		logger = org.apache.log4j.Logger.getLogger("lbmmon");
		org.apache.log4j.BasicConfigurator.configure();
		log4jLogger lbmlogger = new log4jLogger(logger);
		lbm.setLogger(lbmlogger);

		LBMObjectRecyclerBase objRec = new LBMObjectRecycler();
		int transport = LBMMonitor.TRANSPORT_LBM;
		int format = LBMMonitor.FORMAT_CSV;
		String transport_options = "";
		String format_options = "";
		LongOpt[] longopts = new LongOpt[5];
		final int OPTION_MONITOR_TRANSPORT = 4;
		final int OPTION_MONITOR_TRANSPORT_OPTS = 5; 
		final int OPTION_MONITOR_FORMAT = 6;
		final int OPTION_MONITOR_FORMAT_OPTS = 7;
		longopts[0] = new LongOpt("transport", LongOpt.REQUIRED_ARGUMENT, null, OPTION_MONITOR_TRANSPORT);
		longopts[1] = new LongOpt("transport-opts", LongOpt.REQUIRED_ARGUMENT, null, OPTION_MONITOR_TRANSPORT_OPTS);
		longopts[2] = new LongOpt("format", LongOpt.REQUIRED_ARGUMENT, null, OPTION_MONITOR_FORMAT);
		longopts[3] = new LongOpt("format-opts", LongOpt.REQUIRED_ARGUMENT, null, OPTION_MONITOR_FORMAT_OPTS);
		longopts[4] = new LongOpt("help", LongOpt.NO_ARGUMENT, null, 'h');
		Getopt gopt = new Getopt("lbmmon", args, "+t:f:", longopts);
		int c = -1;
		boolean error = false;
		while ((c = gopt.getopt()) != -1)
		{
			switch (c)
			{
				case 'h':
					print_help_exit(0);
				case 'f':
				case OPTION_MONITOR_FORMAT:
					if (gopt.getOptarg().compareToIgnoreCase("csv") == 0)
						format = LBMMonitor.FORMAT_CSV;
					else if (gopt.getOptarg().compareToIgnoreCase("pb") == 0)
						format = LBMMonitor.FORMAT_PB;
					else
						error = true;
					break;
				case 't':
				case OPTION_MONITOR_TRANSPORT:
					if (gopt.getOptarg().compareToIgnoreCase("lbm") == 0)
					{
						transport = LBMMonitor.TRANSPORT_LBM;
					}
					else
					{
						if (gopt.getOptarg().compareToIgnoreCase("udp") == 0)
						{
							transport = LBMMonitor.TRANSPORT_UDP;
						}
						else
						{
							if (gopt.getOptarg().compareToIgnoreCase("lbmsnmp") == 0)
							{
								transport = LBMMonitor.TRANSPORT_LBMSNMP;
							}
							else
							{
								error = true;
							}
						}
					}
					break;
				case OPTION_MONITOR_TRANSPORT_OPTS:
					transport_options += gopt.getOptarg();
					break;
				case OPTION_MONITOR_FORMAT_OPTS:
					format_options += gopt.getOptarg();
					break;
				default:
					error = true;
					break;
			}
			if (error)
				break;
		}
		if (error)
		{
			/* An error occurred processing the command line - print help and exit */
			print_help_exit(1);
		}
		LBMMonitorReceiver lbmmonrcv = null;
		try
		{
			lbmmonrcv = new LBMMonitorReceiver(format, format_options, transport, transport_options, objRec, null);
			//If not using object recycling
			//lbmmonrcv = new LBMMonitorReceiver(format, format_options, transport, transport_options);
		}
		catch (LBMException ex)
		{
			System.err.println("Error creating monitor receiver: " + ex.toString());
			System.exit(1);
		}
		LBMMonCallbacks lbmmoncbs = new LBMMonCallbacks(lbmmonrcv, objRec);
		lbmmonrcv.addStatisticsCallback(lbmmoncbs);
		for (;;)
		{
			try
			{
				Thread.sleep(2000);
			}
			catch (InterruptedException e) { }
		}
	}
	
	private static void print_help_exit(int exit_value)
	{
		System.err.println(LBM.version());
		System.err.println(purpose);
		System.err.println(usage);
		System.exit(exit_value);
	}
}

class LBMMonCallbacks extends LBMMonitorStatisticsCallbackObject
{
	@SuppressWarnings("unused")
	private LBMMonitorReceiver _lbmmonrcv;
	private LBMObjectRecyclerBase _objRec;
	
	LBMMonCallbacks(LBMMonitorReceiver lbmmonrcv, LBMObjectRecyclerBase objRec)
	{
		_lbmmonrcv = lbmmonrcv;
		_objRec = objRec;
	}

	public void onReceive(LBMSourceStatistics statsMsg) {
		try {
			System.err.println(statsMsg.displayString("Source statistics received"));
		} catch (Exception ex) {
			System.err.println("Error printing source statistics: " + ex.toString());
		}
		_objRec.doneWithSourceStatistics(statsMsg);
	}

	public void onReceive(LBMReceiverStatistics statsMsg)
	{
		try {
			System.err.println(statsMsg.displayString("Receiver statistics received"));
		} catch (Exception ex) {
			System.err.println("Error printing receiver statistics: " + ex.toString());
		}
		_objRec.doneWithReceiverStatistics(statsMsg);
	}
	public void onReceive(LBMContextStatistics statsMsg)
	{
		try {
			System.err.println(statsMsg.displayString("Context statistics received"));
		} catch (Exception ex) {
			System.err.println("Error printing context statistics: " + ex.toString());
		}
		_objRec.doneWithContextStatistics(statsMsg);
	}

	public void onReceive(LBMEventQueueStatistics statsMsg) {
		try {
			System.err.println(statsMsg.displayString("Event Queue statistics received"));
		} catch (Exception ex) {
			System.err.println("Error printing event queue statistics: " + ex.toString());
		}
		_objRec.doneWithEventQueueStatistics(statsMsg);
	}

	public void onReceive(LBMImmediateMessageSourceStatistics statsMsg) {
		try {
			System.err.println(statsMsg.displayString("IM source statistics received"));
		} catch (Exception ex) {
			System.err.println("Error printing IM source statistics: " + ex.toString());
		}
		_objRec.doneWithImmediateMessageSourceStatistics(statsMsg);
	}

	public void onReceive(LBMImmediateMessageReceiverStatistics statsMsg) {
		try {
			System.err.println(statsMsg.displayString("IM receiver statistics received"));
		} catch (Exception ex) {
			System.err.println("Error printing IM receiver statistics: " + ex.toString());
		}
		_objRec.doneWithImmediateMessageReceiverStatistics(statsMsg);
	}

	/* Receiving statsMsg messages as binary messages in ByteBuffers */
	public void onReceive(short type, short format, ByteBuffer attributeBuffer, ByteBuffer statsMsg) {
		try {
			if (format == LBMMonitor.FORMAT_CSV) {
				switch (type) {
					case LBMMonitor.LBMMON_PACKET_TYPE_SOURCE:
						if (_lbmmonrcv.getSourceType(attributeBuffer) == LBMMonitor.LBMMON_ATTR_SOURCE_NORMAL) {
							LBMSourceStatistics srcStats = new LBMSourceStatistics(_lbmmonrcv, attributeBuffer, statsMsg);
							System.err.println(srcStats.displayString("Source statistics received"));
						} else {
							LBMImmediateMessageSourceStatistics imsrcStats = new LBMImmediateMessageSourceStatistics(_lbmmonrcv, attributeBuffer, statsMsg);
							System.err.println(imsrcStats.displayString("IM source statistics received"));
						}
						break;
					case LBMMonitor.LBMMON_PACKET_TYPE_RECEIVER:
						if (_lbmmonrcv.getSourceType(attributeBuffer) == LBMMonitor.LBMMON_ATTR_SOURCE_NORMAL) {
							LBMReceiverStatistics rcvStats = new LBMReceiverStatistics(_lbmmonrcv, attributeBuffer, statsMsg);
							System.err.println(rcvStats.displayString("Receiver statistics received"));
						} else {
							LBMImmediateMessageReceiverStatistics rcvStats = new LBMImmediateMessageReceiverStatistics(_lbmmonrcv, attributeBuffer, statsMsg);
							System.err.println(rcvStats.displayString("IM receiver statistics received"));
						}
						break;
					case LBMMonitor.LBMMON_PACKET_TYPE_EVENT_QUEUE:
						LBMEventQueueStatistics eqStats = new LBMEventQueueStatistics(_lbmmonrcv, attributeBuffer, statsMsg);
						System.err.println(eqStats.displayString("Event Queue statistics received"));
						break;
					case LBMMonitor.LBMMON_PACKET_TYPE_CONTEXT:
						LBMContextStatistics ctxStats = new LBMContextStatistics(_lbmmonrcv, attributeBuffer, statsMsg);
						System.err.println(ctxStats.displayString("Context statistics received"));
						break;
					case LBMMonitor.LBMMON_PACKET_TYPE_RECEIVER_TOPIC:
						System.err.println("Receiver Topic statistics received");
						break;
					case LBMMonitor.LBMMON_PACKET_TYPE_WILDCARD_RECEIVER:
						System.err.println("Wildcard Receiver statistics received");
						break;
					default:
						System.err.println("Error: unknown statistics packet type received." + type);
						break;
				}
			} else if (format == LBMMonitor.FORMAT_PB) {
                StringBuilder sb = new StringBuilder();
                UMSMonMsg umsMonMsg;
				UMMonAttributes attrMsg;
				Stats stats;
				Events events;
                LBMStatistics attributes;

				switch (type) {
					case LBMMonitor.LBMMON_PACKET_TYPE_SOURCE:
                        umsMonMsg = UMSMonMsg.parseFrom(statsMsg);
                        attrMsg = umsMonMsg.getAttributes();
                        stats = umsMonMsg.getStats();
                        for (SourceTransport srcMsg : stats.getSourceTransportsList()) {
                            if (srcMsg.getSourceFlag() == SourceTransport.SourceType.SOURCE_NORMAL) {
                                LBMSourceStatistics srcStats = new LBMSourceStatistics(attrMsg, srcMsg);
                                System.err.println(srcStats.displayString("Source statistics received"));
                            } else {
                                LBMImmediateMessageSourceStatistics imsrcStats = new LBMImmediateMessageSourceStatistics(attrMsg, srcMsg);
                                System.err.println(imsrcStats.displayString("IM source statistics received"));
                            }
                        }
						break;
					case LBMMonitor.LBMMON_PACKET_TYPE_RECEIVER:
                        umsMonMsg = UMSMonMsg.parseFrom(statsMsg);
                        attrMsg = umsMonMsg.getAttributes();
						stats = umsMonMsg.getStats();
                        for (ReceiverTransport rcvMsg : stats.getReceiverTransportsList()) {
                            if (rcvMsg.getSourceFlag() == ReceiverTransport.SourceType.SOURCE_NORMAL) {
                                LBMReceiverStatistics rcvStats = new LBMReceiverStatistics(attrMsg, rcvMsg);
                                System.err.println(rcvStats.displayString("Receiver statistics received"));
                            } else {
                                LBMImmediateMessageReceiverStatistics imrcvStats = new LBMImmediateMessageReceiverStatistics(attrMsg, rcvMsg);
                                System.err.println(imrcvStats.displayString("IM receiver statistics received"));
                            }
                        }
						break;
					case LBMMonitor.LBMMON_PACKET_TYPE_EVENT_QUEUE:
                        umsMonMsg = UMSMonMsg.parseFrom(statsMsg);
                        attrMsg = umsMonMsg.getAttributes();
						stats = umsMonMsg.getStats();
                        for (EventQueue eqMsg : stats.getEventQueuesList()) {
                            LBMEventQueueStatistics eqStats = new LBMEventQueueStatistics(attrMsg, eqMsg);
                            System.err.println(eqStats.displayString("Event Queue statistics received"));
                        }
						break;
					case LBMMonitor.LBMMON_PACKET_TYPE_CONTEXT:
                        umsMonMsg = UMSMonMsg.parseFrom(statsMsg);
                        attrMsg = umsMonMsg.getAttributes();
						stats = umsMonMsg.getStats();
						Context ctxMsg = stats.getContext();
						LBMContextStatistics ctxStats = new LBMContextStatistics(attrMsg, ctxMsg);
						System.err.println(ctxStats.displayString("Context statistics received"));
						break;
					case LBMMonitor.LBMMON_PACKET_TYPE_RECEIVER_TOPIC:
					    umsMonMsg = UMSMonMsg.parseFrom(statsMsg);
                        attrMsg = umsMonMsg.getAttributes();
                        attributes = new LBMStatistics(attrMsg);
                        sb.append(attributes.displayString("Receiver Topic events received"));
						events = umsMonMsg.getEvents();
                        for (ReceiverTopic rcvTopicMsg : events.getReceiverTopicsList()) {
							sb.append("\n\tTopic: " + rcvTopicMsg.getTopic());
                            for (ReceiverTopic.Source source : rcvTopicMsg.getSourcesList()) {
                            	Date timestamp = new Date(source.getTimestampSec()*1000);
                            	sb.append("\n\t\tSource           : " + source.getSourceString());
                                sb.append("\n\t\tOTID             : " + source.getOtid());
								sb.append("\n\t\tTopic index      : " + source.getTopicIdx());
								sb.append("\n\t\tSource state     : " + source.getSourceState() + " on " + timestamp + "\n");
                            }
                            System.err.println(sb.toString());
                        }
						break;
					case LBMMonitor.LBMMON_PACKET_TYPE_WILDCARD_RECEIVER:
                        umsMonMsg = UMSMonMsg.parseFrom(statsMsg);
                        attrMsg = umsMonMsg.getAttributes();
                        attributes = new LBMStatistics(attrMsg);
                        sb.append(attributes.displayString("Wildcard Receiver events received"));
                        events = umsMonMsg.getEvents();
                        for (WildcardReceiver wildcardRcvMsg : events.getWildcardReceiversList()) {
                            TextFormat.printer().print(wildcardRcvMsg, sb);
                            System.err.println(sb.toString());
                        }
						break;
					case LBMMonitor.LBMMON_PACKET_TYPE_UMESTORE:
						UMPMonMsg umpMessage = UMPMonMsg.parseFrom(statsMsg);
						attrMsg = umpMessage.getAttributes();
						attributes = new LBMStatistics(attrMsg);
						sb.append(attributes.displayString("UME store statistics received"));
						sb.append("\n");
						if (umpMessage.hasConfigs()) {
							TextFormat.printer().print(umpMessage.getConfigs(), sb);
						}
						if (umpMessage.hasStats()) {
							TextFormat.printer().print(umpMessage.getStats(), sb);
						}
						if (umpMessage.hasEvents()) {
							TextFormat.printer().print(umpMessage.getEvents(), sb);
						}
						System.err.println(sb.toString());
						break;
                    case LBMMonitor.LBMMON_PACKET_TYPE_GATEWAY:
						DROMonMsg droMessage = DROMonMsg.parseFrom(statsMsg);
						attrMsg = droMessage.getAttributes();
						attributes = new LBMStatistics(attrMsg);
						sb.append(attributes.displayString("Gateway statistics received"));
						sb.append("\n");
						if (droMessage.hasConfigs()) {
							TextFormat.printer().print(droMessage.getConfigs(), sb);
						}
						if (droMessage.hasStats()) {
							TextFormat.printer().print(droMessage.getStats(), sb);
						}
                        System.err.println(sb.toString());
                        break;
					default:
						System.err.println("Error: unknown statistics packet type received." + type);
						break;
				}
			} else {
				System.err.println("Error: Received invalid format for passthrough packet");
			}
		} catch (Exception ex) {
			System.err.println("Error processing passthrough statistics packet: " + ex.toString());
		}
	}

    private final static char[] hexArray = "0123456789abcdef".toCharArray();

    private static String bytesToHex(byte[] bytes) {
        char[] hexChars = new char[bytes.length * 2];
        for (int j = 0; j < bytes.length; j++) {
            int v = bytes[j] & 0xFF;
            hexChars[j * 2] = hexArray[v >>> 4];
            hexChars[j * 2 + 1] = hexArray[v & 0x0F];
        }
        return new String(hexChars);
    }
}

