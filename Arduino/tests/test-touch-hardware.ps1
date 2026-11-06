param(
  [string]$Port = 'COM6',
  [ValidateSet('Smoke', 'Capture', 'Sleep', 'Media', 'Startup')]
  [string]$Phase = 'Smoke',
  [int]$CaptureSeconds = 45,
  [ValidateSet('sd', 'flash')]
  [string]$ExpectedStorage = 'sd',
  [ValidateRange(1,320)]
  [int]$MediaWidth = 240,
  [ValidateRange(1,240)]
  [int]$MediaHeight = 240,
  [string]$OutputDirectory = 'tmp/hardware-regression-20261106'
)
$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$log = Join-Path $OutputDirectory ($Phase.ToLowerInvariant() + '.jsonl')
$serial = New-Object System.IO.Ports.SerialPort $Port,921600,'None',8,'One'
$serial.DtrEnable = $false
$serial.RtsEnable = $false
$serial.WriteTimeout = 5000
$versionHeader = Join-Path $PSScriptRoot '../mixer/version.h'
$versionMatch = Select-String -Path $versionHeader -Pattern '#define\s+FW_VERSION\s+"([^"]+)"'
if (-not $versionMatch) { throw 'Cannot determine firmware version from mixer/version.h' }
$expectedVersion = 'fw:esp32-mixer-touch:' + $versionMatch.Matches[0].Groups[1].Value
$script:failures = 0
function Record($name, $passed, $reply, [long]$elapsedMs = -1) {
  [pscustomobject]@{
    time = [DateTimeOffset]::UtcNow.ToOffset([TimeSpan]::FromHours(2)).ToString('o')
    test = $name; passed = $passed; reply = $reply; elapsedMs = $elapsedMs
  } | ConvertTo-Json -Compress | Add-Content -LiteralPath $log
  if ($passed -eq $false) { $script:failures++ }
  Write-Output "$name -> $passed $($reply.Trim())"
}
function Read-For([int]$milliseconds) {
  $clock = [Diagnostics.Stopwatch]::StartNew()
  $text = ''
  while ($clock.ElapsedMilliseconds -lt $milliseconds) {
    $text += $serial.ReadExisting()
    Start-Sleep -Milliseconds 20
  }
  return $text
}
function Query($command, $pattern, $name, [int]$timeout = 5000) {
  $serial.DiscardInBuffer()
  $serial.WriteLine($command)
  $reply = ''
  $clock = [Diagnostics.Stopwatch]::StartNew()
  while ($clock.ElapsedMilliseconds -lt $timeout) {
    $reply += $serial.ReadExisting()
    if ($reply -match $pattern) { break }
    Start-Sleep -Milliseconds 20
  }
  $script:lastReply = $reply
  Record $name ($reply -match $pattern) $reply $clock.ElapsedMilliseconds
}
function Send($command) { $serial.WriteLine($command); Start-Sleep -Milliseconds 40 }
function Fixture {
  Send 'screen:on'
  Send 'cfg:idle:60000'
  Send 'cfg:knobs:5'
  $colors = @('FF0000','00FF00','0000FF','FFFF00','FF00FF')
  for ($i = 1; $i -le 5; $i++) {
    Send "assign:knob${i}:$($colors[$i-1]):a:TEST $i"
    Send "state:knob${i}:0.50:0"
    Send "cfg:discord:knob${i}:1"
    Send "discord:knob${i}:0:0"
  }
  Send 'vol:knob1:0.50'
}
try {
  $serial.Open()
  Start-Sleep -Milliseconds 1500
  Query 'ver?' ([regex]::Escape($expectedVersion)) 'Firmware identity'
  if ($script:failures) { throw 'Unexpected firmware: refusing to run touch fixtures.' }
  if ($Phase -eq 'Smoke') {
    Query 'media?' 'media:320:240' 'Touch native media capability'
    Query 'storage?' "storage:${ExpectedStorage}:(\d+|pending)" "Storage backend: $ExpectedStorage"
    Query 'gif:space?' 'gif:space:(\d+|pending)' 'GIF free-space query'
    foreach ($invalidName in @('', '../invalid', 'a name', 'abcdefghijklmnopqrstuvwxyz')) {
      Query "profile:save:$invalidName" 'profile:err' 'Invalid snapshot name rejected'
    }
    Query 'profile:load:MissingRegression20261106' 'profile:err' 'Missing snapshot rejected'
    Fixture
    Query 'profile:save:Regression20261106' 'profile:ok' "Snapshot saved to $ExpectedStorage"
    Send 'assign:knob1:FFFFFF:a:CHANGED'
    Query 'profile:load:Regression20261106' 'profile:ok' "Snapshot loaded from $ExpectedStorage"
    # These commands should be ignored safely; the following version query confirms liveness.
    Send 'vol:knob0:0.1'
    Send 'mute:knob99:1'
    Send 'discord:knob1:2:0'
    Send 'discord:knob1:0:1junk'
    Send 'unknown:regression'
    Query 'ver?' ([regex]::Escape($expectedVersion)) 'Alive after invalid commands'
    $serial.Write('ve'); Start-Sleep -Milliseconds 100
    $serial.Write("r?`r`n")
    $reply = Read-For 500
    Record 'Fragmented CRLF command replies once' (($reply.Trim() -eq $expectedVersion)) $reply
    Send 'screen:off'
    Query 'ver?' ([regex]::Escape($expectedVersion)) 'Serial responds while screen is off'
    Send 'screen:on'
    Query 'ver?' ([regex]::Escape($expectedVersion)) 'Serial responds after screen resumes'
  } elseif ($Phase -eq 'Startup') {
    # Run immediately after reset/flash. Existing idle media must be present.
    Send 'cfg:idle:1000'
    Send 'vol:knob1:0.50'
    $startupClock = [Diagnostics.Stopwatch]::StartNew()
    $known = $false
    while ($startupClock.ElapsedMilliseconds -lt 180000) {
      Query 'ver?' ([regex]::Escape($expectedVersion)) 'Version responds during free-space count' 2500
      Query 'gif:space?' 'gif:space:(\d+|pending)' 'Media budget responds during free-space count' 2500
      Query 'storage?' "storage:${ExpectedStorage}:(\d+|pending)" 'Storage responds during free-space count' 2500
      if ($script:lastReply -match "storage:${ExpectedStorage}:\d+") { $known = $true; break }
      if ($script:failures) { break }
      Start-Sleep -Milliseconds 250
    }
    Record 'Free-space count eventually completes' $known "$($startupClock.ElapsedMilliseconds) ms"
  } elseif ($Phase -eq 'Capture') {
    Fixture
    Write-Output "Touch capture ready for $CaptureSeconds seconds"
    $clock = [Diagnostics.Stopwatch]::StartNew()
    $reply = ''
    $pending = ''
    while ($clock.ElapsedMilliseconds -lt $CaptureSeconds * 1000) {
      $received = $serial.ReadExisting()
      $reply += $received
      $pending += $received
      while ($pending.Contains("`n")) {
        $end = $pending.IndexOf("`n")
        [pscustomobject]@{ elapsedMs = $clock.ElapsedMilliseconds; event = $pending.Substring(0, $end).Trim() } |
          ConvertTo-Json -Compress | Add-Content (Join-Path $OutputDirectory 'touch-events.jsonl')
        $pending = $pending.Substring($end + 1)
      }
      Start-Sleep -Milliseconds 20
    }
    Record 'Physical touch events (requires human review)' $null $reply
  } elseif ($Phase -eq 'Sleep') {
    Fixture
    Start-Sleep -Seconds 4
    Send 'screen:off'
    Write-Output 'Screen and LED should now be OFF for 10 seconds.'
    Query 'ver?' ([regex]::Escape($expectedVersion)) 'Sleeping board responds'
    Send 'state:knob1:0.75:1'
    Start-Sleep -Seconds 10
    Send 'screen:on'
    Write-Output 'Screen should now be ON and LED green.'
    Query 'ver?' ([regex]::Escape($expectedVersion)) 'Resumed board responds'
  } elseif ($Phase -eq 'Media') {
    # Replaces existing idle media. Caller must intend a destructive media regression.
    Query 'gif:clear' 'gif:cleared' 'Clear idle media'
    Query 'gif:d:AAAA' 'gif:err' 'Data outside upload rejected'
    Query 'gif:end' 'gif:err' 'End outside upload rejected'
    Query 'gif:begin:65:240:240:100' 'gif:err' 'Too many frames rejected'
    Query 'gif:begin:1:321:240:100' 'gif:err' 'Oversized width rejected'
    Query 'gif:begin:1:320:241:100' 'gif:err' 'Oversized height rejected'
    Query 'gif:begin:1:240:240:100' 'gif:rdy' 'Start interrupted upload'
    Query 'gif:d:APgA+A==' 'gif:ack' 'Partial frame acknowledged'
    Query 'gif:end' 'gif:err' 'Incomplete frame rejected'
    Query "gif:begin:2:${MediaWidth}:${MediaHeight}:500,500" 'gif:rdy' 'Start two-frame upload'
    $frame = New-Object byte[] ($MediaWidth * $MediaHeight * 2)
    foreach ($color in @(0xF800, 0x001F)) {
      for ($i = 0; $i -lt $frame.Length; $i += 2) {
        $frame[$i] = $color -band 255
        $frame[$i + 1] = ($color -shr 8) -band 255
      }
      for ($offset = 0; $offset -lt $frame.Length; $offset += 1024) {
        $count = [Math]::Min(1024, $frame.Length - $offset)
        $payload = [Convert]::ToBase64String($frame, $offset, $count)
        Query "gif:d:$payload" 'gif:ack' 'Media chunk acknowledged'
        if ($script:failures) { throw 'Media transfer failed' }
      }
    }
    Query 'gif:end' 'gif:done' 'Finish complete media upload'
    Send 'cfg:idle:1000'
    Send 'vol:knob1:0.50'
    Write-Output 'Idle playback should alternate red and blue twice per second.'
    Start-Sleep -Seconds 12
  }
} finally {
  if ($serial.IsOpen) {
    try { $serial.WriteLine('screen:on') } finally { $serial.Close() }
  }
  $serial.Dispose()
}
if ($script:failures) { throw "$script:failures hardware checks failed; see $log" }
