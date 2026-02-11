#!/usr/bin/env python3
# Copyright (c) 2026-present The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the -rpcstripfield option."""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.test_node import ErrorMatch
from test_framework.util import assert_equal
from test_framework.wallet import MiniWallet


class RpcStripFieldTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True
        self.extra_args = [[
            "-txindex",
            "-rpcstripfield=getrawtransaction:blockhash",
            "-rpcstripfield=getrawtransaction:vout[].scriptPubKey.address",
        ]]

    def run_test(self):
        wallet = MiniWallet(self.nodes[0])
        wallet.generate(101, called_by_framework=True)
        txid, blockhash = self.create_confirmed_tx(wallet)

        self.log.info("Verify method-specific rule and [] selector")
        tx = self.nodes[0].getrawtransaction(txid=txid, verbosity=1, blockhash=blockhash)
        assert "blockhash" not in tx
        for out in tx["vout"]:
            assert "address" not in out["scriptPubKey"]
        assert_equal(tx["txid"], txid)

        self.log.info("Verify [0] selector only removes first element field")
        self.restart_node(0, extra_args=[
            "-txindex",
            "-rpcstripfield=getrawtransaction:vout[0].scriptPubKey.address",
        ])
        tx = self.nodes[0].getrawtransaction(txid=txid, verbosity=1, blockhash=blockhash)
        assert "address" not in tx["vout"][0]["scriptPubKey"]
        if len(tx["vout"]) > 1:
            assert "address" in tx["vout"][1]["scriptPubKey"]

        self.log.info("Verify global (*) and method-specific rules can be combined")
        self.restart_node(0, extra_args=[
            "-txindex",
            "-rpcstripfield=*:txid",
            "-rpcstripfield=getrawtransaction:hex",
        ])
        tx = self.nodes[0].getrawtransaction(txid=txid, verbosity=1, blockhash=blockhash)
        assert "txid" not in tx
        assert "hex" not in tx

        self.log.info("Verify unknown path is ignored (no-op)")
        self.restart_node(0, extra_args=[
            "-txindex",
            "-rpcstripfield=getrawtransaction:does.not.exist",
        ])
        tx = self.nodes[0].getrawtransaction(txid=txid, verbosity=1, blockhash=blockhash)
        assert_equal(tx["txid"], txid)
        assert "hex" in tx
        assert "blockhash" in tx

        self.log.info("Verify invalid -rpcstripfield value fails during init")
        self.stop_node(0)
        self.nodes[0].assert_start_raises_init_error(
            extra_args=["-rpcstripfield=getrawtransaction"],
            expected_msg="Invalid -rpcstripfield value",
            match=ErrorMatch.PARTIAL_REGEX,
        )

    def create_confirmed_tx(self, wallet):
        txid = wallet.send_self_transfer(from_node=self.nodes[0])["txid"]
        [blockhash] = self.generate(self.nodes[0], 1)
        return txid, blockhash


if __name__ == '__main__':
    RpcStripFieldTest(__file__).main()
