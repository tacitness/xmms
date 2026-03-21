chore: rename default branch from master to main
The default branch should be `main` per modern conventions.

## Changes required
1. Rename local branch: `git branch -m master main`
2. Push `main` to remote
3. Delete `master` remote branch
4. Update GitHub default branch to `main`
5. Update any CI/CD workflows that reference `master`
