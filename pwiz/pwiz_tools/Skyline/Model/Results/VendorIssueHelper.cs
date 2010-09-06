﻿/*
 * Original author: Brendan MacLean <brendanx .at. u.washington.edu>,
 *                  MacCoss Lab, Department of Genome Sciences, UW
 *
 * Copyright 2010 University of Washington - Seattle, WA
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
using System;
using System.Diagnostics;
using System.IO;
using System.Linq;
using pwiz.Skyline.Util;

namespace pwiz.Skyline.Model.Results
{
    internal static class VendorIssueHelper
    {
        private const string EXE_MZ_WIFF = "mzWiff";
        private const string EXT_WIFF_SCAN = ".scan";

        public static string CreateTempFileSubstitute(string filePath, int sampleIndex,
            LoadingTooSlowlyException.Solution workAround, ILoadMonitor loader, ref ProgressStatus status)
        {
            string tempFileSubsitute = Path.GetTempFileName();

            try
            {
                switch (workAround)
                {
                    case LoadingTooSlowlyException.Solution.local_file:
                        loader.UpdateProgress(status = status.ChangeMessage(string.Format("Local copy work-around for {0}", Path.GetFileName(filePath))));
                        File.Copy(filePath, tempFileSubsitute, true);
                        break;
                    case LoadingTooSlowlyException.Solution.mzwiff_conversion:
                        loader.UpdateProgress(status = status.ChangeMessage(string.Format("Convert to mzXML work-around for {0}", Path.GetFileName(filePath))));
                        ConvertWiffToMzxml(filePath, sampleIndex, tempFileSubsitute, loader);
                        break;
                }

                return tempFileSubsitute;
            }
            catch (Exception)
            {
                FileEx.DeleteIfPossible(tempFileSubsitute);
                throw;
            }
        }

        private static void ConvertWiffToMzxml(string filePathWiff, int sampleIndex,
            string outputPath, IProgressMonitor monitor)
        {
            // The WIFF file needs to be on the local file system for the conversion
            // to work.  So, just in case it is on a network share, copy it to the
            // temp directory.
            string tempFileSource = Path.GetTempFileName();
            try
            {
                File.Copy(filePathWiff, tempFileSource, true);
                string filePathScan = GetWiffScanPath(filePathWiff);
                if (File.Exists(filePathScan))
                    File.Copy(filePathScan, GetWiffScanPath(tempFileSource));
                ConvertLocalWiffToMzxml(tempFileSource, sampleIndex, outputPath, monitor);
            }
            finally
            {
                FileEx.DeleteIfPossible(tempFileSource);
                FileEx.DeleteIfPossible(GetWiffScanPath(tempFileSource));
            }
        }

        private static string GetWiffScanPath(string filePathWiff)
        {
            return filePathWiff + EXT_WIFF_SCAN;
        }

        private static void ConvertLocalWiffToMzxml(string filePathWiff, int sampleIndex,
            string outputPath, IProgressMonitor monitor)
        {
            if (AdvApi.GetPathFromProgId("Analyst.ChromData") == null)
            {
                throw new IOException(string.Format("The file {0} cannot be imported by the AB Sciex WiffFileDataReader library in a reasonable time frame. " +
                    "To work around this issue requires Analyst to be installed on the computer running {1}.\n" +
                    "Please install Analyst, or run this import on a computure with Analyst installed", filePathWiff, Program.Name));
            }

            var argv = new[]
                           {
                               "--mzXML",
                               "-s" + sampleIndex,
                               "\"" + filePathWiff + "\"",
                               "\"" + outputPath + "\"",
                           };

            var psi = new ProcessStartInfo(EXE_MZ_WIFF)
            {
                CreateNoWindow = true,
                UseShellExecute = false,
                // Common directory includes the directory separator
                WorkingDirectory = Path.GetDirectoryName(filePathWiff),
                Arguments = string.Join(" ", argv.ToArray()),
                RedirectStandardError = true,
                RedirectStandardOutput = true,
            };

            var proc = Process.Start(psi);
            if (proc != null)
            {
                // For debugging mzWiff
//                var reader = new ProcessStreamReader(proc);
//                string line;
//                while ((line = reader.ReadLine()) != null)
//                    Console.WriteLine(line);

                while (!proc.WaitForExit(200))
                {
                    if (monitor.IsCanceled)
                    {
                        proc.Kill();
                        throw new LoadCanceledException(new ProgressStatus("").Cancel());
                    }
                }
            }

            if (proc == null || proc.ExitCode != 0)
            {
                throw new IOException(string.Format("Failure attempting to convert sample {0} in {1} to mzXML to work around a performance issue in the AB Sciex WiffFileDataReader library.",
                    sampleIndex, filePathWiff));
            }
        }
    }

    internal class LoadingTooSlowlyException : IOException
    {
        public enum Solution { local_file, mzwiff_conversion }

        public LoadingTooSlowlyException(Solution solution, ProgressStatus status, double predictedMinutes, double maximumMinutes)
            : base(string.Format("Data import expected to consume {0} minutes with maximum of {1} mintues",
                                 predictedMinutes, maximumMinutes))
        {
            WorkAround = solution;
            Status = status;
        }

        public Solution WorkAround { get; private set; }
        public ProgressStatus Status { get; private set; }
    }
}