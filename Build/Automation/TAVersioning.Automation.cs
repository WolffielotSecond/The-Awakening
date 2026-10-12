using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Text;
using Microsoft.Extensions.Logging;

/// <summary>Stamp the filtered staging config, even when packaging skips compilation.</summary>
[CustomStageCopyHandler("TAVersioning")]
public sealed class TAVersionStageCopyHandler : CustomStageCopyHandler
{
    private readonly Dictionary<string, string> BuildIds = new(StringComparer.OrdinalIgnoreCase);

    public override bool CanCopyFile(string sourceName) =>
        string.Equals(Path.GetFileName(sourceName), "DefaultGame.ini", StringComparison.OrdinalIgnoreCase);

    public override bool StageFile(ILogger logger, string sourceName, string targetName)
    {
        if (!CanCopyFile(sourceName)) { return false; }
        string projectRoot = FindProjectRoot(sourceName);
        string text = File.ReadAllText(sourceName);
        string buildId = ReadBuildId(text);
        if (BuildIds.TryGetValue(projectRoot, out string? existingId))
        {
            // The same config can be staged more than once for different chunks.
            File.WriteAllText(targetName, ReplaceBuildId(text, existingId), new UTF8Encoding(false));
            return true;
        }
        if (!string.Equals(buildId, "local", StringComparison.OrdinalIgnoreCase))
        {
            // Prepare / Package already reserved an identity; never allocate a second one.
            BuildIds[projectRoot] = buildId;
            logger.LogInformation("TA Versioning: retaining prepared BuildId={BuildId}", buildId);
            return false;
        }

        string scriptPath = Path.Combine(projectRoot, "Tools", "Versioning", "Manage-Version.ps1");
        if (!string.Equals(Path.GetFullPath(sourceName), Path.GetFullPath(targetName), StringComparison.OrdinalIgnoreCase))
        {
            File.Copy(sourceName, targetName, true);
        }
        // Windows PowerShell ships with Windows; no dependency on a user's pwsh PATH.
        string powershell = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.System), "WindowsPowerShell", "v1.0", "powershell.exe");
        ProcessStartInfo startInfo = new(powershell)
        {
            UseShellExecute = false,
            CreateNoWindow = true,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            WorkingDirectory = projectRoot
        };
        foreach (string argument in new[] { "-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", "-File", scriptPath,
            "-Action", "Stage", "-ProjectRoot", projectRoot, "-StagedIniPath", targetName })
        {
            startInfo.ArgumentList.Add(argument);
        }
        using Process process = Process.Start(startInfo) ?? throw new InvalidOperationException("Cannot start version stamping script.");
        var output = process.StandardOutput.ReadToEndAsync();
        var errors = process.StandardError.ReadToEndAsync();
        if (!process.WaitForExit(60000))
        {
            process.Kill(true);
            throw new InvalidOperationException("TA Versioning: script timed out; packaging stopped.");
        }
        if (process.ExitCode != 0)
        {
            throw new InvalidOperationException($"TA Versioning: script failed (exit {process.ExitCode}): {errors.GetAwaiter().GetResult()}");
        }
        buildId = ReadBuildId(File.ReadAllText(targetName));
        if (string.Equals(buildId, "local", StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidOperationException("TA Versioning: staging config still has BuildId=local.");
        }
        BuildIds[projectRoot] = buildId;
        logger.LogInformation("TA Versioning: staged BuildId={BuildId}. {Output}", buildId, output.GetAwaiter().GetResult().Trim());
        return true;
    }

    private static string FindProjectRoot(string sourceName)
    {
        for (DirectoryInfo? directory = new FileInfo(sourceName).Directory; directory != null; directory = directory.Parent)
        {
            if (File.Exists(Path.Combine(directory.FullName, "The_Awakening.uproject")) &&
                File.Exists(Path.Combine(directory.FullName, "Tools", "Versioning", "Manage-Version.ps1")))
            {
                return directory.FullName;
            }
        }
        throw new InvalidOperationException($"Cannot locate versioning project for {sourceName}.");
    }

    private static string ReadBuildId(string text)
    {
        bool inside = false;
        foreach (string line in text.Split('\n'))
        {
            string trimmed = line.Trim();
            if (trimmed.StartsWith('[')) { inside = trimmed.Equals("[TheAwakening.Build]", StringComparison.OrdinalIgnoreCase); }
            else if (inside && trimmed.StartsWith("BuildId=", StringComparison.OrdinalIgnoreCase))
            {
                string value = trimmed.Substring("BuildId=".Length).Trim();
                if (value.Length > 0) { return value; }
            }
        }
        throw new InvalidOperationException("TA Versioning: missing BuildId in staging config.");
    }

    private static string ReplaceBuildId(string text, string buildId)
    {
        string newline = text.Contains("\r\n", StringComparison.Ordinal) ? "\r\n" : "\n";
        string[] lines = text.Replace("\r\n", "\n", StringComparison.Ordinal).Split('\n');
        bool inside = false;
        for (int i = 0; i < lines.Length; i++)
        {
            string trimmed = lines[i].Trim();
            if (trimmed.StartsWith('[')) { inside = trimmed.Equals("[TheAwakening.Build]", StringComparison.OrdinalIgnoreCase); }
            else if (inside && trimmed.StartsWith("BuildId=", StringComparison.OrdinalIgnoreCase)) { lines[i] = "BuildId=" + buildId; }
        }
        return string.Join(newline, lines);
    }
}
