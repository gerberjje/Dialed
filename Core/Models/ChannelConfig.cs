using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Serialization;

namespace Dialed.Core.Models;

public class ChannelConfig : IJsonOnDeserialized
{
    public int KnobIndex { get; set; }
    public string AppName { get; set; } = "";

    // Read older multi-app settings once; new saves use the upstream AppName field.
    [JsonIgnore(Condition = JsonIgnoreCondition.WhenWritingNull)]
    public List<string>? AppNames { get; set; }

    void IJsonOnDeserialized.OnDeserialized()
    {
        if (string.IsNullOrWhiteSpace(AppName))
            AppName = AppNames?.FirstOrDefault(name => !string.IsNullOrWhiteSpace(name)) ?? "Select App";
        AppNames = null;
    }
}
