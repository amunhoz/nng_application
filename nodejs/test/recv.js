const broker = require("../index")
const client = new broker();

//let connc = client.connect("ipc:///tmp/testx.ipc")
let connc = client.connect("tcp://localhost:8000")

if (!connc) console.log("errorc")

var totRcv=0
let subs = client.subscribe("test");
client.onData( (channel, buff)=>{
    totRcv+=1
    if (totRcv % 1000 ==0 ) 
        console.log("received", totRcv)
});