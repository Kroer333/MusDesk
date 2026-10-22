param([string]$OutputDirectory = (Join-Path $PSScriptRoot '../build/demo'))
$ErrorActionPreference = 'Stop'
$demoDirectory = [IO.Path]::GetFullPath($OutputDirectory)
[IO.Directory]::CreateDirectory($demoDirectory) | Out-Null
Add-Type -AssemblyName System.Drawing
$drawingAssemblies = @([System.Drawing.Bitmap].Assembly.Location, [System.Drawing.Point].Assembly.Location,
    [System.ComponentModel.Component].Assembly.Location) | Select-Object -Unique
foreach ($assemblyName in @('System.Private.Windows.GdiPlus', 'System.Private.Windows.Core', 'System.Runtime')) {
    $assemblyPath = Join-Path $PSHOME "$assemblyName.dll"
    if (Test-Path -LiteralPath $assemblyPath) { $drawingAssemblies += $assemblyPath }
}
Add-Type -ReferencedAssemblies $drawingAssemblies -TypeDefinition @'
using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.IO;
public static class MusDeskDemoAssets {
    public static void Write(string directory) {
        var random = new Random(42);
        using (var image = new Bitmap(720, 1280))
        using (var graphics = Graphics.FromImage(image)) {
            graphics.SmoothingMode = SmoothingMode.AntiAlias;
            using (var gradient = new LinearGradientBrush(new Point(0, 0), new Point(0, 1280),
                Color.FromArgb(5, 12, 32), Color.FromArgb(32, 67, 86)))
                graphics.FillRectangle(gradient, 0, 0, 720, 1280);
            for (int i = 0; i < 230; ++i) {
                int x = random.Next(720), y = random.Next(750), alpha = random.Next(60, 210);
                using (var brush = new SolidBrush(Color.FromArgb(alpha, 188, 224, 255)))
                    graphics.FillEllipse(brush, x, y, i % 7 == 0 ? 3 : 1.5f, i % 7 == 0 ? 3 : 1.5f);
            }
            for (int radius = 190; radius >= 60; radius -= 4)
                using (var brush = new SolidBrush(Color.FromArgb(3, 129, 192, 240)))
                    graphics.FillEllipse(brush, 520 - radius, 220 - radius, radius * 2, radius * 2);
            using (var moon = new SolidBrush(Color.FromArgb(224, 241, 248)))
                graphics.FillEllipse(moon, 462, 162, 116, 116);
            using (var hills = new SolidBrush(Color.FromArgb(8, 22, 39)))
                graphics.FillPolygon(hills, new [] {new Point(0,760),new Point(0,610),new Point(145,455),
                    new Point(310,715),new Point(460,580),new Point(720,770)});
            using (var sea = new LinearGradientBrush(new Point(0,760),new Point(0,1280),
                Color.FromArgb(13,48,65),Color.FromArgb(3,12,29)))
                graphics.FillRectangle(sea,0,760,720,520);
            for (int i = 0; i < 520; ++i) {
                int y = random.Next(770,1280), x = random.Next(720);
                double reflection = Math.Exp(-Math.Pow((x-520.0)/(35+(y-760)*0.28),2));
                using (var pen = new Pen(Color.FromArgb((int)(12+reflection*95),155,208,224),1))
                    graphics.DrawLine(pen,x,y,x+random.Next(4,35),y);
            }
            image.Save(Path.Combine(directory,"musdesk-demo.png"),ImageFormat.Png);
        }
        const int rate = 48000, seconds = 12;
        using (var file = new BinaryWriter(File.Create(Path.Combine(directory,"musdesk-demo.wav")))) {
            file.Write(System.Text.Encoding.ASCII.GetBytes("RIFF")); file.Write(36+rate*seconds*4);
            file.Write(System.Text.Encoding.ASCII.GetBytes("WAVEfmt ")); file.Write(16);
            file.Write((short)1); file.Write((short)2); file.Write(rate); file.Write(rate*4);
            file.Write((short)4); file.Write((short)16); file.Write(System.Text.Encoding.ASCII.GetBytes("data"));
            file.Write(rate*seconds*4);
            for (int i = 0; i < rate*seconds; ++i) {
                double t = i/(double)rate, beat = t % 0.5;
                double fade = Math.Min(1,Math.Min(t/0.04,(seconds-t)/0.2));
                double envelope = Math.Exp(-beat*12)*(1-Math.Exp(-beat*150));
                double tone = Math.Sin(t*2*Math.PI*220)*0.18+Math.Sin(t*2*Math.PI*330)*0.08;
                short sample = (short)(tone*envelope*fade*32767);
                file.Write(sample); file.Write(sample);
            }
        }
    }
}
'@
[MusDeskDemoAssets]::Write($demoDirectory)
Write-Output "Generated original artwork and synthesized audio in $demoDirectory"
