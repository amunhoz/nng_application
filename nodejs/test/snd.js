const broker = require("../index")
const server = new broker();

//let conns = server.listen("ipc:///tmp/testx.ipc")
let conns = server.listen("tcp://localhost:8000")
if (!conns) console.log("error")
let data = Buffer.from("lalalalala")


var totSnd=0;  
setTimeout(()=>{        
    setInterval(()=>{
        totSnd+=1
        server.publish("test", data)      
        if (totSnd % 1000 ==0 ) 
            console.log("sended", totSnd)  
    
    }, 1) 
    
}, 10000)
