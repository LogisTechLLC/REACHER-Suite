import { ReactiveESM, ReactiveESMView } from "./reactive_esm";
class AnyWidgetModelAdapter {
    static __name__ = "AnyWidgetModelAdapter";
    constructor(model) {
        this.view = null;
        this.model = model;
        this.model_changes = {};
        this.data_changes = {};
    }
    get(name) {
        let value;
        if (name in this.model.data.attributes) {
            value = this.model.data.attributes[name];
        }
        else {
            value = this.model.attributes[name];
        }
        if (value instanceof ArrayBuffer) {
            value = new DataView(value);
        }
        return value;
    }
    set(name, value) {
        if (name in this.model.data.attributes) {
            this.data_changes = { ...this.data_changes, [name]: value };
        }
        else if (name in this.model.attributes) {
            this.model_changes = { ...this.model_changes, [name]: value };
        }
    }
    save_changes() {
        this.model.setv(this.model_changes);
        this.model_changes = {};
        this.model.data.setv(this.data_changes);
        this.data_changes = {};
    }
    on(event, cb) {
        if (event.startsWith("change:")) {
            this.model.watch(this.view, event.slice("change:".length), cb);
        }
        else if (event === "msg:custom" && this.view) {
            this.view.on_event(cb);
        }
        else {
            console.error(`Event of type '${event}' not recognized.`);
        }
    }
    off(event, cb) {
        if (event.startsWith("change:")) {
            this.model.unwatch(this.view, event.slice("change:".length), cb);
        }
        else if (event === "msg:custom" && this.view) {
            this.view.remove_on_event(cb);
        }
        else {
            console.error(`Event of type '${event}' not recognized.`);
        }
    }
}
class AnyWidgetAdapter extends AnyWidgetModelAdapter {
    static __name__ = "AnyWidgetAdapter";
    constructor(view) {
        super(view.model);
        this.view = view;
    }
    get_child(name) {
        const child_model = this.model.data[name];
        if (Array.isArray(child_model)) {
            const subchildren = [];
            for (const subchild of child_model) {
                const subview = this.view.get_child_view(subchild);
                if (subview) {
                    subchildren.push(subview.el);
                }
            }
            return subchildren;
        }
        else {
            return this.view.get_child_view(child_model)?.el;
        }
    }
}
export class AnyWidgetComponentView extends ReactiveESMView {
    static __name__ = "AnyWidgetComponentView";
    adapter;
    destroyer;
    initialize() {
        super.initialize();
        this.adapter = new AnyWidgetAdapter(this);
    }
    remove() {
        super.remove();
        if (this.destroyer) {
            this.destroyer.then((d) => d({ model: this.adapter, el: this.container }));
        }
    }
    _render_code() {
        return `
const view = Bokeh.index.find_one_by_id('${this.model.id}')

function render() {
  const out = Promise.resolve(view.render_fn({
    view, model: view.adapter, data: view.model.data, el: view.container
  }) || null)
  view.destroyer = out
  out.then(() => view.after_rendered())
}

export default {render}`;
    }
    after_rendered() {
        this.render_children();
        this._rendered = true;
    }
}
export class AnyWidgetComponent extends ReactiveESM {
    static __name__ = "AnyWidgetComponent";
    constructor(attrs) {
        super(attrs);
    }
    _run_initializer(initialize) {
        const props = { model: new AnyWidgetModelAdapter(this) };
        initialize(props);
    }
    static __module__ = "panel.models.esm";
    static {
        this.prototype.default_view = AnyWidgetComponentView;
    }
}
//# sourceMappingURL=anywidget_component.js.map