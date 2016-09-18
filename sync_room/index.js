var app = require('express')();
var http = require('http').Server(app);
var io = require('socket.io')(http);

var crypto = require('crypto')
var base64url = require('base64url');

function randBytesAsBase64Url(size) {
	return base64url(crypto.randomBytes(size))
}

app.get('/', function(req, res) {
	res.sendFile(__dirname + '/index.html');
});

io.on('connection', function(socket) {
	console.log('a user connected');
	socket.on('disconnect', function() {
		console.log('user disconnected');
	});

	socket.on('create_room', function(name) {
		io.emit('room_added', name); // Broadcast to all
		// The one who created the room joins it automatically
		socket.join(name);
		console.log('Room "' + name + '" created');
		console.log('User ' + socket.id + ' joined "' + name + '"');
	});

	socket.on('join_room', function(name) {
		socket.join(name);
		// TODO: leave other rooms using io.sockets.manager.roomClients[socket.id]
		console.log('User ' + socket.id + ' joined "' + name + '"');
	});
});

http.listen(4200, function() {
	console.log('listening on *:4200');
});
