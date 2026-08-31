@Component({selector: "app"})
class Service {
  private count: number = 0;

  @Input()
  async fetch(url: string): Promise<Data> {
    return await get(url);
  }

  validate(x: number): boolean {
    return true;
  }
}
