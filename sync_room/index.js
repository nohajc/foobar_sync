var app = require('express')();
var http = require('http').Server(app);
var io = require('socket.io')(http);

var crypto = require('crypto')
var base64url = require('base64url');

function randBytesAsBase64Url(size) {
	return base64url(crypto.randomBytes(size))
}

function joinRoomExclusively(socket, name) {
	if (socket.last_joined != undefined) {
		socket.leave(socket.last_joined);
		console.log('User ' + socket.id + ' left "' + socket.last_joined + '"');
	}
	socket.join(name);
	socket.last_joined = name;
	console.log('User ' + socket.id + ' joined "' + name + '"');
}

app.get('/', function(req, res) {
	res.sendFile(__dirname + '/index.html');
});

io.on('connection', function(socket) {
	console.log('a user connected');
	socket.on('disconnect', function() {
		console.log('user disconnected');
		// Broadcast updated room list.
		// Rooms occupied by the disconnected user only are removed.
		io.emit('room_list', io.sockets.adapter.rooms);
		console.log(io.sockets.adapter.rooms);
	});

	socket.on('create_room', function(name) {
		io.emit('room_added', name); // Broadcast to all
		// The one who created the room joins it automatically
		console.log('Room "' + name + '" created');
		joinRoomExclusively(socket, name);
	});

	socket.on('join_room', function(name) {
		joinRoomExclusively(socket, name);
	});

	socket.on('list_rooms', function() {
		socket.emit('room_list', io.sockets.adapter.rooms);
		console.log(io.sockets.adapter.rooms);
	});
});

http.listen(4200, function() {
	console.log('listening on *:4200');
});
