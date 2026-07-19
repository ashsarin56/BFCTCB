const express = require('express');
const net = require('net');
const path = require('path');

const app = express();
const port = 3000;
const SOCKET_PATH = '/tmp/gateway_admin.sock';

app.use(express.static(path.join(__dirname, 'public')));

function queryGateway(command) {
    return new Promise((resolve, reject) => {
        const client = net.createConnection(SOCKET_PATH, () => {
            client.write(command);
        });

        let data = '';
        client.on('data', (chunk) => {
            data += chunk.toString();
        });

        client.on('end', () => {
            try {
                const json = JSON.parse(data);
                resolve(json);
            } catch (err) {
                console.error("Failed to parse gateway response:", data);
                resolve({ error: "parse_error", raw: data });
            }
        });

        client.on('error', (err) => {
            reject(err);
        });
    });
}

app.get('/api/metrics', async (req, res) => {
    try {
        const data = await queryGateway('GET_METRICS');
        res.json(data);
    } catch (err) {
        res.status(500).json({ error: 'Gateway unavailable' });
    }
});

app.get('/api/topology', async (req, res) => {
    try {
        const data = await queryGateway('GET_TOPOLOGY');
        res.json(data);
    } catch (err) {
        res.status(500).json({ error: 'Gateway unavailable' });
    }
});

app.get('/api/events', async (req, res) => {
    try {
        const data = await queryGateway('GET_EVENTS');
        res.json(data);
    } catch (err) {
        res.status(500).json({ error: 'Gateway unavailable' });
    }
});

app.listen(port, () => {
    console.log(`Gateway Dashboard running at http://localhost:${port}`);
});
