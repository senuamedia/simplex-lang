package tree_sitter_simplex_test

import (
	"testing"

	tree_sitter "github.com/smacker/go-tree-sitter"
	"github.com/tree-sitter/tree-sitter-simplex"
)

func TestCanLoadGrammar(t *testing.T) {
	language := tree_sitter.NewLanguage(tree_sitter_simplex.Language())
	if language == nil {
		t.Errorf("Error loading Simplex grammar")
	}
}
