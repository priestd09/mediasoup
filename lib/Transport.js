'use strict';

const EventEmitter = require('events').EventEmitter;

const logger = require('./logger')('Transport');
const utils = require('./utils');
const errors = require('./errors');

class Transport extends EventEmitter
{
	constructor(internal, data, channel)
	{
		logger.debug('constructor() [internal:%o, data:%o]', internal, data);

		super();

		// Room identificator
		this._roomId = internal.roomId;

		// Peer identificator
		this._peerId = internal.peerId;

		// Transport identificator
		this._transportId = internal.transportId;

		// Transport data provided by the worker
		this._data = data;

		// Channel instance
		this._channel = channel;

		// Closed flag
		this._closed = false;

		// TODO: subscribe to channel notifications from the worker
	}

	get closed()
	{
		return this._closed;
	}

	get iceComponent()
	{
		return this._data.iceComponent;
	}

	get iceLocalParameters()
	{
		return this._data.iceLocalParameters;
	}

	get iceLocalCandidates()
	{
		return this._data.iceLocalCandidates;
	}

	get dtlsLocalFingerprints()
	{
		return this._data.dtlsLocalFingerprints;
	}

	/**
	 * Close the Transport
	 */
	close(error, dontSendChannel)
	{
		if (!error)
			logger.debug('close()');
		else
			logger.error('close() [error:%s]', error);

		if (this._closed)
			return;

		this._closed = true;

		if (!dontSendChannel)
		{
			let internal =
			{
				roomId      : this._roomId,
				peerId      : this._peerId,
				transportId : this._transportId
			};

			// Send Channel request
			this._channel.request('transport.close', internal)
				.then(() =>
				{
					logger.debug('"transport.close" request succeeded');
				})
				.catch((error) =>
				{
					logger.error('"transport.close" request failed [status:%s, error:"%s"]', error.status, error.message);
				});
		}

		this.emit('close', error);
	}

	dump()
	{
		logger.debug('dump()');

		if (this._closed)
			return Promise.reject(errors.Closed('transport closed'));

		let internal =
		{
			roomId      : this._roomId,
			peerId      : this._peerId,
			transportId : this._transportId
		};

		return this._channel.request('transport.dump', internal)
			.then((data) =>
			{
				logger.debug('"transport.dump" request succeeded');

				return data;
			})
			.catch((error) =>
			{
				logger.error('"transport.dump" request failed [status:%s, error:"%s"]', error.status, error.message);

				throw error;
			});
	}

	/**
	 * Provide the remote DTLS parameters
	 * @param  {Object} options  Remote DTLS parameters
	 * @return {Promise}
	 */
	setRemoteDtlsParameters(options)
	{
		logger.debug('setRemoteDtlsParameters() [options:%o]', options);

		if (this._closed)
			return Promise.reject(errors.Closed('transport closed'));

		let internal =
		{
			roomId      : this._roomId,
			peerId      : this._peerId,
			transportId : this._transportId
		};

		// Send Channel request
		return this._channel.request('transport.setRemoteDtlsParameters', internal, options)
			.then((data) =>
			{
				logger.debug('"transport.setRemoteDtlsParameters" request succeeded');

				// TODO: Handle content of data and append it to this._data.

				return data;
			})
			.catch((error) =>
			{
				logger.error('"transport.setRemoteDtlsParameters" request failed [status:%s, error:"%s"]', error.status, error.message);

				throw error;
			});
	}

	/**
	 * Create an asociated Transport for RTCP
	 * @return {Promise}
	 */
	createAssociatedTransport()
	{
		logger.debug('createAssociatedTransport()');

		if (this._closed)
			return Promise.reject(errors.Closed('transport closed'));

		let internal =
		{
			roomId         : this._roomId,
			peerId         : this._peerId,
			transportId    : utils.randomNumber(),
			rtpTransportId : this._transportId
		};

		return this._channel.request('peer.createAssociatedTransport', internal)
			.then((data) =>
			{
				logger.debug('"peer.createAssociatedTransport" request succeeded');

				// Create a Transport instance
				let transport = new Transport(internal, data, this._channel);

				// Emit 'associatedtransport' so the owner Peer can handle this new
				// transport
				this.emit('associatedtransport', transport);

				return transport;
			})
			.catch((error) =>
			{
				logger.error('"peer.createAssociatedTransport" request failed [status:%s, error:"%s"]', error.status, error.message);

				throw error;
			});
	}
}

module.exports = Transport;