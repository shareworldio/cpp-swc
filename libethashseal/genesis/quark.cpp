#include "../GenesisInfo.h"

static std::string const c_genesisInfoQuark = std::string() +
R"E(
{
	"sealEngine": "QPOS",
	"params": {
		"syncRequest": "0x01",
		"accountStartNonce": "0x00",
		"homesteadForkBlock": "0x00",
		"daoHardforkBlock": "0x00",
		"EIP150ForkBlock": "0x00",
		"EIP158ForkBlock": "0x00",
		"byzantiumForkBlock": "0x00",
		"constantinopleForkBlock": "0x00",
		"networkID" : "0xfe",
		"chainID": "0xfe",
		"maximumExtraDataSize": "0x100",
		"tieBreakingGas": false,
		"minGasLimit": "0x1388",
		"maxGasLimit": "7fffffffffffffff",
		"gasLimitBoundDivisor": "0x0400",
		"minimumDifficulty": "0x020000",
		"difficultyBoundDivisor": "0x0800",
		"durationLimit": "0x0d",
		"blockReward": "0x00"
	},
	"genesis": {
		"nonce": "0x0000000000000042",
		"difficulty": "0x0",
		"mixHash": "0x0000000000000000000000000000000000000000000000000000000000000000",
		"author": "0x0000000000000000000000000000000000000000",
		"timestamp": "0x00",
		"parentHash": "0x0000000000000000000000000000000000000000000000000000000000000000",
		"extraData": "0x11bbe8db4e347b4e8c937c1c8370e4b5ed33adb3db69cbdb7a38e1e50b1b82fa",
		"gasLimit": "0x1000000"
	},
	"accounts": {
		"0000000000000000000000000000000000000001": { "precompiled": { "name": "ecrecover", "linear": { "base": 3000, "word": 0 } } },
		"0000000000000000000000000000000000000002": { "precompiled": { "name": "sha256", "linear": { "base": 60, "word": 12 } } },
		"0000000000000000000000000000000000000003": { "precompiled": { "name": "ripemd160", "linear": { "base": 600, "word": 120 } } },
		"0000000000000000000000000000000000000004": { "precompiled": { "name": "identity", "linear": { "base": 15, "word": 3 } } },
		"0000000000000000000000000000000000000005": { "precompiled": { "name": "modexp", "startingBlock" : "0x2dc6c0" } },
		"0000000000000000000000000000000000000006": { "precompiled": { "name": "alt_bn128_G1_add", "startingBlock" : "0x2dc6c0", "linear": { "base": 500, "word": 0 } } },
		"0000000000000000000000000000000000000007": { "precompiled": { "name": "alt_bn128_G1_mul", "startingBlock" : "0x2dc6c0", "linear": { "base": 40000, "word": 0 } } },
		"0000000000000000000000000000000000000008": { "precompiled": { "name": "alt_bn128_pairing_product", "startingBlock" : "0x2dc6c0" } },
		"0x02d6ac8ba56b8c7c0084daaaf3a7a381f7a3e004": {"balance": "1337000000000000000000000000000000000000000000000000"},
		"0x34b6cc4da59ac6378df18826ce1517d4d91f464a": {"balance": "1337000000000000000000000000000000000000000000000000"},
		"0x0c5b587Ca140De48d5eaDaF0e2ED051Fa31AF749": {"balance": "1337000000000000000000000000000000000000000000000000"},
		"0xbd30Bb846A187DEF21ACaC7f4A1317c97fB98197": {"balance": "1337000000"}
	}
}
)E";
