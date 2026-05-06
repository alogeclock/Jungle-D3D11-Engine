local Log = {}

function Log.IsEnabled(config)
    return config ~= nil and config.debug ~= nil and config.debug.enable_log == true
end

function Log.Write(config, message, prefix)
    if Log.IsEnabled(config) then
        print((prefix or "") .. tostring(message))
    end
end

function Log.Log(config, message, prefix)
    Log.Write(config, message, prefix)
end

function Log.MakeLogger(config, prefix)
    return function(message)
        Log.Write(config, message, prefix)
    end
end

return Log
