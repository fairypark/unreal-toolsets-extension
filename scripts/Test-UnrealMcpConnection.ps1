[CmdletBinding()]
param(
    [ValidatePattern('^http://(127\.0\.0\.1|localhost):[0-9]+/[A-Za-z0-9._~!$&''()*+,;=:@%/-]+$')]
    [string] $Endpoint = 'http://127.0.0.1:8000/mcp',

    [ValidatePattern('^[A-Za-z0-9_.]+$')]
    [string] $ToolsetName = '',

    [ValidateNotNull()]
    [string[]] $RequiredOperationNames = @()
)

$ErrorActionPreference = 'Stop'

$NormalizedRequiredOperationNames = @(
    $RequiredOperationNames |
        ForEach-Object { $_ -split ',' } |
        ForEach-Object { $_.Trim() } |
        Where-Object { $_ }
)

Add-Type -AssemblyName System.Net.Http

function ConvertFrom-McpResponseBody {
    param(
        [Parameter(Mandatory)]
        [string] $Body
    )

    $TrimmedBody = $Body.Trim()
    if ($TrimmedBody.StartsWith('{')) {
        return $TrimmedBody | ConvertFrom-Json
    }

    foreach ($Line in $TrimmedBody -split "`r?`n") {
        if ($Line.StartsWith('data:')) {
            $Data = $Line.Substring(5).Trim()
            if ($Data.StartsWith('{')) {
                return $Data | ConvertFrom-Json
            }
        }
    }

    throw 'The MCP server returned neither a JSON response nor a JSON SSE data event.'
}

function Send-McpRequest {
    param(
        [Parameter(Mandatory)]
        [System.Net.Http.HttpClient] $Client,

        [Parameter(Mandatory)]
        [string] $Body,

        [string] $SessionId = '',

        [string] $Operation = 'MCP request'
    )

    $Request = [System.Net.Http.HttpRequestMessage]::new(
        [System.Net.Http.HttpMethod]::Post,
        $Endpoint
    )
    [void] $Request.Headers.TryAddWithoutValidation(
        'Accept',
        'application/json, text/event-stream'
    )
    if ($SessionId) {
        [void] $Request.Headers.TryAddWithoutValidation(
            'Mcp-Session-Id',
            $SessionId
        )
    }
    $Request.Content = [System.Net.Http.StringContent]::new(
        $Body,
        [System.Text.Encoding]::UTF8,
        'application/json'
    )

    try {
        $Response = $Client.SendAsync($Request).GetAwaiter().GetResult()
    }
    catch {
        throw "$Operation could not reach the MCP endpoint."
    }
    finally {
        $Request.Dispose()
    }
    if (-not $Response.IsSuccessStatusCode) {
        $StatusCode = [int] $Response.StatusCode
        $Response.Dispose()
        throw "$Operation failed with HTTP status $StatusCode."
    }

    return $Response
}

function Invoke-McpTool {
    param(
        [Parameter(Mandatory)]
        [System.Net.Http.HttpClient] $Client,

        [Parameter(Mandatory)]
        [string] $SessionId,

        [Parameter(Mandatory)]
        [int] $RequestId,

        [Parameter(Mandatory)]
        [string] $ToolName,

        [Parameter(Mandatory)]
        [hashtable] $Arguments
    )

    $Body = @{
        jsonrpc = '2.0'
        id = $RequestId
        method = 'tools/call'
        params = @{
            name = $ToolName
            arguments = $Arguments
        }
    } | ConvertTo-Json -Depth 8 -Compress
    $Response = Send-McpRequest `
        -Client $Client `
        -Body $Body `
        -SessionId $SessionId `
        -Operation $ToolName
    try {
        $Payload = ConvertFrom-McpResponseBody (
            $Response.Content.ReadAsStringAsync().GetAwaiter().GetResult()
        )
    }
    finally {
        $Response.Dispose()
    }

    if ($Payload.error) {
        throw "$ToolName returned JSON-RPC error $($Payload.error.code)."
    }
    if ($Payload.id -ne $RequestId -or $Payload.result -isnot [PSCustomObject] -or
        $Payload.result.content -isnot [array]) {
        throw "$ToolName returned a malformed tool result."
    }
    if ($Payload.result.isError -eq $true) {
        throw "$ToolName returned an MCP tool error."
    }

    return $Payload
}

function ConvertFrom-McpToolText {
    param(
        [Parameter(Mandatory)]
        $Payload,

        [Parameter(Mandatory)]
        [string] $Operation
    )

    $TextContent = @(
        $Payload.result.content |
            Where-Object { $_.type -eq 'text' } |
            Select-Object -First 1
    )
    if ($TextContent.Count -ne 1 -or -not $TextContent[0].text) {
        throw "$Operation returned no text result."
    }
    try {
        return $TextContent[0].text | ConvertFrom-Json
    }
    catch {
        throw "$Operation returned text that is not JSON."
    }
}

$Handler = [System.Net.Http.HttpClientHandler]::new()
$Handler.AllowAutoRedirect = $false
$Client = [System.Net.Http.HttpClient]::new($Handler)
$Client.Timeout = [TimeSpan]::FromSeconds(10)

try {
    $InitializeBody = @{
        jsonrpc = '2.0'
        id = 1
        method = 'initialize'
        params = @{
            protocolVersion = '2025-03-26'
            capabilities = @{}
            clientInfo = @{
                name = 'UnrealToolsetsExtensionConnectionTest'
                version = '1.0'
            }
        }
    } | ConvertTo-Json -Depth 8 -Compress

    $InitializeResponse = Send-McpRequest `
        -Client $Client `
        -Body $InitializeBody `
        -Operation 'initialize'
    try {
        $InitializePayload = ConvertFrom-McpResponseBody (
            $InitializeResponse.Content.ReadAsStringAsync().GetAwaiter().GetResult()
        )
        $SessionId = (
            $InitializeResponse.Headers.GetValues('Mcp-Session-Id') |
                Select-Object -First 1
        )
    }
    finally {
        $InitializeResponse.Dispose()
    }

    if (-not $SessionId) {
        throw 'The MCP initialize response did not include Mcp-Session-Id.'
    }

    $InitializedBody = @{
        jsonrpc = '2.0'
        method = 'notifications/initialized'
        params = @{}
    } | ConvertTo-Json -Depth 4 -Compress
    $InitializedResponse = Send-McpRequest `
        -Client $Client `
        -Body $InitializedBody `
        -SessionId $SessionId `
        -Operation 'notifications/initialized'
    $InitializedResponse.Dispose()

    $ToolsListBody = @{
        jsonrpc = '2.0'
        id = 2
        method = 'tools/list'
        params = @{}
    } | ConvertTo-Json -Depth 4 -Compress
    $ToolsListResponse = Send-McpRequest `
        -Client $Client `
        -Body $ToolsListBody `
        -SessionId $SessionId `
        -Operation 'tools/list'
    try {
        $ToolsListPayload = ConvertFrom-McpResponseBody (
            $ToolsListResponse.Content.ReadAsStringAsync().GetAwaiter().GetResult()
        )
    }
    finally {
        $ToolsListResponse.Dispose()
    }

    if ($ToolsListPayload.error) {
        throw "tools/list returned JSON-RPC error $($ToolsListPayload.error.code)."
    }
    if ($ToolsListPayload.result.tools -isnot [array]) {
        throw 'tools/list returned a malformed tools array.'
    }
    $ToolNames = @($ToolsListPayload.result.tools | ForEach-Object { $_.name })
    # Discover the protocol capability, not the engine/server version string.
    $DescribeToolName = if ('describe_toolsets' -in $ToolNames) {
        'describe_toolsets'
    } elseif ('describe_toolset' -in $ToolNames) {
        'describe_toolset'
    } else {
        throw 'MCP tool-search meta-tools are missing: describe_toolset or describe_toolsets.'
    }
    $RequiredMetaToolNames = @('list_toolsets', $DescribeToolName, 'call_tool')
    $MissingToolNames = @($RequiredMetaToolNames | Where-Object { $_ -notin $ToolNames })
    if ($MissingToolNames.Count -gt 0) {
        throw "MCP tool-search meta-tools are missing: $($MissingToolNames -join ', ')."
    }

    $CatalogPayload = Invoke-McpTool `
        -Client $Client `
        -SessionId $SessionId `
        -RequestId 3 `
        -ToolName 'list_toolsets' `
        -Arguments @{}
    # Epic's catalog is plain text in both supported versions, not JSON.
    $CatalogNames = @(
        foreach ($Content in $CatalogPayload.result.content) {
            if ($Content.type -eq 'text') {
                foreach ($Line in ($Content.text -split "`r?`n")) {
                    if ($Line -match '^- ([A-Za-z0-9_.]+)(?::|$)') { $Matches[1] }
                }
            }
        }
    )
    if ($CatalogNames.Count -eq 0) {
        throw 'list_toolsets returned no recognizable toolset names.'
    }
    $DescribedToolsetName = if ($ToolsetName) {
        $ToolsetName
    }
    else {
        'ToolsetRegistry.AgentSkillToolset'
    }
    if ($DescribedToolsetName -cnotin $CatalogNames) {
        # Use exact advertised class names. Similar names across engine releases
        # do not imply identical operation contracts.
        throw "Requested toolset is not advertised: $DescribedToolsetName. Use list_toolsets to discover its current qualified name."
    }
    $DescribeArguments = if ($DescribeToolName -eq 'describe_toolsets') {
        @{ toolset_names = @($DescribedToolsetName); include_schemas = $true }
    } else {
        @{ toolset_name = $DescribedToolsetName }
    }
    $DescribePayload = Invoke-McpTool `
        -Client $Client `
        -SessionId $SessionId `
        -RequestId 4 `
        -ToolName $DescribeToolName `
        -Arguments $DescribeArguments
    $Description = ConvertFrom-McpToolText `
        -Payload $DescribePayload `
        -Operation $DescribeToolName
    if ($DescribeToolName -eq 'describe_toolsets') {
        if ($Description.toolsets -isnot [PSCustomObject] -or
            $Description.errors -isnot [PSCustomObject]) {
            throw 'describe_toolsets returned a malformed response wrapper.'
        }
        if (@($Description.errors.PSObject.Properties).Count -gt 0) {
            throw 'describe_toolsets returned per-toolset errors.'
        }
        $Entry = $Description.toolsets.PSObject.Properties |
            Where-Object { $_.Name -ceq $DescribedToolsetName }
        if (-not $Entry) {
            throw 'describe_toolsets did not return the requested toolset.'
        }
        $Description = $Entry.Value
    }
    if ($Description -isnot [PSCustomObject] -or $Description.tools -isnot [array]) {
        throw "$DescribeToolName returned a malformed toolset description."
    }
    foreach ($Tool in $Description.tools) {
        $Name = if ($Tool.name) { $Tool.name } else { $Tool.tool_name }
        if ($Name -isnot [string] -or -not $Name) {
            throw "$DescribeToolName returned an operation without a name."
        }
    }
    if ($NormalizedRequiredOperationNames.Count -gt 0) {
        $DescribedOperationNames = @(
            $Description.tools | ForEach-Object {
                if ($_.name) { $_.name } elseif ($_.tool_name) { $_.tool_name }
            }
        )
        $MissingOperationNames = @(
            $NormalizedRequiredOperationNames |
                Where-Object {
                    $RequiredName = $_
                    -not ($DescribedOperationNames | Where-Object {
                        $_ -ceq $RequiredName -or
                            ($RequiredName -notlike '*.*' -and $_ -ceq "$DescribedToolsetName.$RequiredName")
                    })
                }
        )
        if ($MissingOperationNames.Count -gt 0) {
            throw "Required operations are missing from ${DescribedToolsetName}: $($MissingOperationNames -join ', ')."
        }
    }
    [void] (Invoke-McpTool `
        -Client $Client `
        -SessionId $SessionId `
        -RequestId 5 `
        -ToolName 'call_tool' `
        -Arguments @{
            tool_name = 'list_toolsets'
            arguments = @{}
        })

    [PSCustomObject] @{
        Endpoint = $Endpoint
        ServerName = $InitializePayload.result.serverInfo.name
        ServerVersion = $InitializePayload.result.serverInfo.version
        ToolSearchMetaTools = $RequiredMetaToolNames
        DescribedToolset = $DescribedToolsetName
        RequiredOperations = $NormalizedRequiredOperationNames
        VerifiedOperations = $RequiredMetaToolNames
    }
}
finally {
    $Client.Dispose()
}
