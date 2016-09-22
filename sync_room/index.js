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

var cached_pieces = {}
var torrent_seeders = {}

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
		//io.emit('room_added', name); // Broadcast to all
		// The one who created the room joins it automatically
		console.log('Room "' + name + '" created');
		joinRoomExclusively(socket, name);
		io.emit('room_list', io.sockets.adapter.rooms);
	});

	socket.on('join_room', function(name) {
		joinRoomExclusively(socket, name);
		io.emit('room_list', io.sockets.adapter.rooms);
	});

	socket.on('list_rooms', function() {
		socket.emit('room_list', io.sockets.adapter.rooms);
		console.log(io.sockets.adapter.rooms);
	});

	socket.on('share_torrent', function(room, torrent_id, data) {
		cached_pieces[torrent_id] = [];
		// Broadcast torrent, associate it with a seeder id
		socket.broadcast.to(room).emit('add_torrent', data);
		torrent_seeders[torrent_id] = socket.id;

		console.log(socket.id + ' shared torrent ' + torrent_id + ' with ' + room);
	});

	socket.on('download_piece', function(torrent_id, piece_idx) {
		// TODO: Get torrent piece from seeder.
		// We either send 'upload_piece' message to seeder
		// or send back result cached here on the server.
		// Caching is going to be temporary:
		// Each piece can be requested at most n-1 times,
		// where n is the number of clients sharing a room.
		// Furthermore we can assume that if one client needs
		// a particular piece, the others are going to ask
		// for it at the same time (since they should be in sync)
		// unless they have already gotten the piece via torrent.
		piece_idx = +piece_idx; // Convert to integer
		var piece = cached_pieces[torrent_id][piece_idx];
		console.log('Piece ' + piece_idx + ' requested');

		if (piece != undefined) {
			console.log(' -> Sending it back from cache');
			socket.emit('piece_downloaded', torrent_id, piece_idx, piece);
		}
		else {
			console.log(' -> Request upload from seeder');
			var seeder_id = torrent_seeders[torrent_id];
			socket.broadcast.to(seeder_id).emit('upload_piece', torrent_id, piece_idx, socket.id);
		}
	});

	socket.on('piece_uploaded', function(torrent_id, piece_idx, piece, recipient_id) {
		piece_idx = +piece_idx; // Convert to integer
		// Cache the result and send it to recipient
		cached_pieces[torrent_id][piece_idx] = piece; // TODO: set timeout to clean the cache record
		socket.broadcast.to(recipient_id).emit('piece_downloaded', torrent_id, piece_idx, piece);
		console.log(' -> Piece ' + piece_idx + ' sent to peer.');
	});
});

http.listen(4200, function() {
	console.log('listening on *:4200');
});
