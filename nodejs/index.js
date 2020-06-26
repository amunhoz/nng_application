const binding = require('./nngcom.node');
class nngbroker{
    constructor (options){
        if (!options) options={}
        this.instance = new binding.nngbroker(JSON.stringify(options))        
    }
    listen(address){
        return this.instance.listen(address)
    }
    connect(address){
        return this.instance.connect(address)
    }
    subscribe(channel){
        return this.instance.subscribe(channel)
    }
    unsubscribe(channel, subid) {
        if (!subid) subid = ""
        return this.instance.unsubscribe(channel, subid)
    }
    publish(channel, buff) {
        return this.instance.publish(channel, buff)
    }
    onData(func) {
        this.instance.ondata(func)
    }
}
module.exports = nngbroker