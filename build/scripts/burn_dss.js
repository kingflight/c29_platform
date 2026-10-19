/*
 * burn_dss.js
 * Usage:
 *   dss.sh burn_dss.js <ccxml> <core-name> <program-out>
 */

var args = this.arguments;

if (args.length < 3)
{
    java.lang.System.err.println("Usage: burn_dss.js <ccxml> <core-name> <program-out>");
    java.lang.System.exit(2);
}

var ccxml = args[0];
var coreName = args[1];
var programOut = args[2];

var env = Packages.com.ti.ccstudio.scripting.environment.ScriptingEnvironment.instance();
env.traceSetConsoleLevel(Packages.com.ti.ccstudio.scripting.environment.TraceLevel.INFO);
env.setScriptTimeout(-1);

var debugServer = null;
var debugSession = null;

function fail(msg, code)
{
    java.lang.System.err.println("ERROR: " + msg);
    cleanup();
    java.lang.System.exit(code);
}

function cleanup()
{
    try
    {
        if (debugSession != null)
        {
            debugSession.terminate();
        }
    }
    catch (e1) {}

    try
    {
        if (debugServer != null)
        {
            debugServer.stop();
        }
    }
    catch (e2) {}
}

try
{
    env.traceWrite("DSS burn start");

    debugServer = env.getServer("DebugServer.1");
    debugServer.setConfig(ccxml);

    debugSession = debugServer.openSession("*", coreName);
    env.traceWrite("Session opened: " + coreName);

    debugSession.target.connect();
    env.traceWrite("Target connected");

    // Reset before program load to start from a known state.
    debugSession.target.reset();

    debugSession.memory.loadProgram(programOut);
    env.traceWrite("Program loaded: " + programOut);

    cleanup();
    env.traceWrite("DSS burn complete");
    java.lang.System.exit(0);
}
catch (ex)
{
    fail(ex, 3);
}
